#include "__dep__.h"
#include "constants.h"
#include "tx.h"
#include "scheduler.h"
#include "rcc/graph.h"
#include "rcc/graph_marshaler.h"
#include "marshal-value.h"
#include "procedure.h"
#include "rcc_rpc.h"
#include "frame.h"
#include "bench/tpcc/workload.h"
#include "executor.h"
#include "coordinator.h"
#include "../bench/rw/workload.h"

#include <gperftools/profiler.h>

namespace janus {

shared_ptr<Tx> TxLogServer::CreateTx(epoch_t epoch, txnid_t tid, bool
read_only) {
  Log_debug("create tid %ld", tid);
  verify(dtxns_.find(tid) == dtxns_.end());
  if (epoch == 0) {
    epoch = epoch_mgr_.curr_epoch_;
  }
  verify(epoch_mgr_.IsActive(epoch));
  auto dtxn = frame_->CreateTx(epoch, tid, read_only, this);
  if (dtxn != nullptr) {
    dtxns_[tid] = dtxn;
    dtxn->recorder_ = this->recorder_;
    dtxn->txn_reg_ = txn_reg_;
    verify(txn_reg_ != nullptr);
    verify(dtxn->tid_ == tid);
  } else {
    verify(0);
  }
  if (epoch_enabled_) {
    epoch_mgr_.AddToEpoch(epoch, tid);
    TriggerUpgradeEpoch();
  }
  dtxn->sched_ = this;
  return dtxn;
}

shared_ptr<Tx> TxLogServer::CreateTx(txnid_t tx_id, bool ro) {
  Log_debug("create tid %" PRIx64, tx_id);
  verify(dtxns_.find(tx_id) == dtxns_.end());
  auto dtxn = frame_->CreateTx(epoch_mgr_.curr_epoch_, tx_id, ro, this);
  if (dtxn != nullptr) {
    dtxns_[tx_id] = dtxn;
    dtxn->recorder_ = this->recorder_;
    verify(txn_reg_);
    dtxn->txn_reg_ = txn_reg_;
    verify(dtxn->tid_ == tx_id);
    if (epoch_enabled_) {
      epoch_mgr_.AddToCurrent(tx_id);
      TriggerUpgradeEpoch();
    }
    dtxn->sched_ = this;
  } else {
    // for multi-paxos this would happen.
    // verify(0);
  }
  return dtxn;
}

shared_ptr<Tx> TxLogServer::GetOrCreateTx(txnid_t tid, bool ro) {
  //Log_info("The current server is %d", site_id_);
  shared_ptr<Tx> ret = nullptr;
  auto it = dtxns_.find(tid);
  if (it == dtxns_.end()) {
    ret = CreateTx(tid, ro);
  } else {
    ret = it->second;
  }
  //Log_info("Tx is %ld", tid);
  verify(ret != nullptr);
  verify(ret->tid_ == tid);
  return ret;
}
void TxLogServer::DestroyTx(i64 tid) {
  Log_debug("destroy tid %lx", tid);
  auto it = dtxns_.find(tid);
  // verify(it != dtxns_.end());
  if (it != dtxns_.end()) {
    dtxns_.erase(it);
  }
}

shared_ptr<Tx> TxLogServer::GetTx(txnid_t tid) {
  // Log_debug("DTxnMgr::get(%ld)\n", tid);
  auto it = dtxns_.find(tid);
  // verify(it != dtxns_.end());
  if (it != dtxns_.end()) {
    return it->second;
  } else {
    return nullptr;
  }
}

mdb::Txn *TxLogServer::GetMTxn(const i64 tid) {
  mdb::Txn *txn = nullptr;
  auto it = mdb_txns_.find(tid);
  if (it == mdb_txns_.end()) {
    verify(0);
  } else {
    txn = it->second;
  }
  return txn;
}

mdb::Txn *TxLogServer::RemoveMTxn(const i64 tid) {
  mdb::Txn *txn = nullptr;
  auto it = mdb_txns_.find(tid);
  verify(it != mdb_txns_.end());
  txn = it->second;
  mdb_txns_.erase(it);
  return txn;
}

mdb::Txn *TxLogServer::GetOrCreateMTxn(const i64 tid) {
  mdb::Txn *txn = nullptr;
  auto it = mdb_txns_.find(tid);
  if (it == mdb_txns_.end()) {
    txn = mdb_txn_mgr_->start(tid);
    // using occ lazy mode: increment version at commit time
    auto mode = Config::GetConfig()->tx_proto_;
    if (mode == MODE_OCC || mode == MODE_MDCC) {
      ((mdb::TxnOCC *) txn)->set_policy(mdb::OCC_LAZY);
    }
    auto ret = mdb_txns_.insert(std::pair<i64, mdb::Txn *>(tid, txn));
    verify(ret.second);
  } else {
    txn = it->second;
  }

  if (IS_MODE_2PL) {
    verify(mdb_txn_mgr_->rtti() == mdb::symbol_t::TXN_2PL);
    verify(txn->rtti() == mdb::symbol_t::TXN_2PL);
  } else {

  }
  verify(txn != nullptr);
  return txn;
}

// TODO move this to the dtxn class
void TxLogServer::get_prepare_log(i64 txn_id,
                                  const std::vector<i32> &sids,
                                  std::string *str) {
  auto it = mdb_txns_.find(txn_id);
  verify(it != mdb_txns_.end() && it->second != NULL);

  // marshal txn_id
  uint64_t len = str->size();
  str->resize(len + sizeof(txn_id));
  memcpy((void *) (str->data()), (void *) (&txn_id), sizeof(txn_id));
  len += sizeof(txn_id);
  verify(len == str->size());

  // p denotes prepare log
  const char prepare_tag = 'p';
  str->resize(len + sizeof(prepare_tag));
  memcpy((void *) (str->data() + len),
         (void *) &prepare_tag,
         sizeof(prepare_tag));
  len += sizeof(prepare_tag);
  verify(len == str->size());

  // marshal related servers
  uint32_t num_servers = sids.size();
  str->resize(len + sizeof(num_servers) + sizeof(i32) * num_servers);
  memcpy((void *) (str->data() + len),
         (void *) &num_servers,
         sizeof(num_servers));
  len += sizeof(num_servers);
  for (uint32_t i = 0; i < num_servers; i++) {
    memcpy((void *) (str->data() + len), (void *) (&(sids[i])), sizeof(i32));
    len += sizeof(i32);
  }
  verify(len == str->size());

  switch (mode_) {
    case MODE_2PL:
    case MODE_OCC:((mdb::Txn2PL *) it->second)->marshal_stage(*str);
      break;
    default:verify(0);
  }
}

TxLogServer::TxLogServer() : mtx_(), curp_coordinator_commit_finish_timeout_pool_(this) {
  mdb_txn_mgr_ = make_shared<mdb::TxnMgrUnsafe>();
  if (Config::GetConfig()->do_logging()) {
    auto path = Config::GetConfig()->log_path();
    // TODO free this
//    recorder_ = new Recorder(path);
  }
}

Coordinator *TxLogServer::CreateRepCoord(const i64& dep_id) {
  Coordinator *coord;
  static cooid_t cid = 0;
  int32_t benchmark = 0;
  static id_t id = 0;
  verify(rep_frame_ != nullptr);
  coord = rep_frame_->CreateCoordinator(cid++,
                                        Config::GetConfig(),
                                        benchmark,
                                        nullptr,
                                        id++,
                                        txn_reg_);
  coord->frame_ = rep_frame_;
  coord->dep_id_ = dep_id;
  coord->par_id_ = partition_id_;
  //Log_info("Partition id set: %d", partition_id_);
  coord->loc_id_ = this->loc_id_;
  coord->dep_id_ = dep_id;
  return coord;
}

// Coordinator *TxLogServer::CreateCurpRepCoord(const i64& dep_id) {
//   Coordinator *coord;
//   static cooid_t cid = 0;
//   int32_t benchmark = 0;
//   static id_t id = 0;
//   verify(curp_rep_frame_ != nullptr);
//   coord = curp_rep_frame_->CreateCoordinator(cid++,
//                                               Config::GetConfig(),
//                                               benchmark,
//                                               nullptr,
//                                               id++,
//                                               txn_reg_);
//   coord->frame_ = curp_rep_frame_;
//   coord->dep_id_ = dep_id;
//   coord->par_id_ = partition_id_;
//   //Log_info("Partition id set: %d", partition_id_);
//   coord->loc_id_ = this->loc_id_;
//   coord->dep_id_ = dep_id;
//   return coord;
// }

TxLogServer::TxLogServer(int mode) : TxLogServer() {
  mode_ = mode;
  switch (mode) {
    case MODE_MDCC:
    case MODE_OCC:
      mdb_txn_mgr_ = make_shared<mdb::TxnMgrOCC>();
      break;
    case MODE_NONE:
    case MODE_RPC_NULL:
    case MODE_RCC:
    case MODE_RO6:
      mdb_txn_mgr_ = make_shared<mdb::TxnMgrUnsafe>();
      break;
    default:verify(0);
  }
}

TxLogServer::~TxLogServer() {
  auto it = mdb_txns_.begin();
  for (; it != mdb_txns_.end(); it++)
    Log::info("tid: %ld still running", it->first);
  if (it != mdb_txns_.end() && it->second) {
    delete it->second;
    it->second = NULL;
  }
  mdb_txns_.clear();
#ifdef CPU_PROFILE_SEVER
  if (site_id_ == 0) {
    ProfilerStop();
    Log_info("[CURP] End Profile");
  }
#endif
  Log_info("svr%d fastpath_timeout_count %d wait_commit_timeout_count %d instance_commit_timeout_trigger_prepare_count %d finish_countdown_count %d", 
    loc_id_, curp_fastpath_timeout_count_, curp_wait_commit_timeout_count_, curp_instance_commit_timeout_trigger_prepare_count_, finish_countdown_count_);
  // [CURP] comment this because FINISH involved and influenced the counting, so only client end counting have been outputed
  // verify(cli2svr_dispatch_count > 0 && cli2svr_commit_count > 0);
  // if (cli2svr_dispatch.count() || cli2svr_commit.count())
  //   Log_info("[CURP] loc_id_=%d site_id_=%d \
  //   curp_fast_path_success_count_=%d curp_coordinator_accept_count_=%d original_protocol_submit_count_=%d total=%d \
  //   cli2svr_dispatch 50% = %.2f ms cli2svr_dispatch 90% = %.2f ms cli2svr_dispatch 99% = %.2f ms \
  //   cli2svr_commit 50% = %.2f ms cli2svr_commit 90% = %.2f ms cli2svr_commit_max 99% = %.2f ms",
  //             loc_id_, site_id_, 
  //             curp_fast_path_success_count_, curp_coordinator_accept_count_, original_protocol_submit_count_,
  //             curp_fast_path_success_count_ + curp_coordinator_accept_count_ + original_protocol_submit_count_,
  //             cli2svr_dispatch.pct50(), cli2svr_dispatch.pct90(), cli2svr_dispatch.pct99(),
  //             cli2svr_commit.pct50(), cli2svr_commit.pct90(), cli2svr_commit.pct99());
  // else
  //   Log_info("[CURP] loc_id_=%d site_id_=%d No Count / Latency Measured", loc_id_, site_id_);
  // if (curp_log_cols_[0] != nullptr)
  //   curp_log_cols_[0]->Print();
  Log_info("loc_id=%d curp_double_commit_count_=%d", loc_id_, curp_double_commit_count_);
  std::vector<double> witness_size_distribution = witness_.witness_size_distribution();
  Log_info("loc_id=%d witness size distribution 50pct %.2f 90pct %.2f 99pct %.2f ave %.2f",
    loc_id_, witness_size_distribution[0], witness_size_distribution[1], witness_size_distribution[2], witness_size_distribution[3]);
#ifdef WITNESS_LOG_DEBUG
  if (loc_id_ == 0 || loc_id_ == 1)
    witness_.print_log();
#endif

#ifdef LATENCY_DEBUG
  Log_info("loc_id=%d cli2preskip_begin_ %.2f cli2preskip_end_ %.2f cli2skip_begin_ %.2f cli2skip_end_ %.2f", loc_id_, cli2preskip_begin_.pct50(), cli2preskip_end_.pct50(), cli2skip_begin_.pct50(), cli2skip_end_.pct50());
  Log_info("loc_id=%d cli2leader_recv_ %.2f cli2leader_send_ %.2f cli2follower_recv_ %.2f cli2follower_send_ %.2f cli2commit_send_ %.2f cli2oncommit_ %.2f",
            loc_id_, cli2leader_recv_.pct50(), cli2leader_send_.pct50(), cli2follower_recv_.pct50(), cli2follower_send_.pct50(), cli2commit_send_.pct50(), cli2oncommit_.pct50());
  Log_info("loc_id=%d preskip_attemp_ %.2f", loc_id_, preskip_attemp_.pct50());
#endif
}

/**
 *
 * @param txn_box
 * @param inn_id, if 0, execute all pieces.
 */
void TxLogServer::Execute(Tx &txn_box,
                          innid_t inn_id) {
  if (inn_id == 0) {
    for (auto &pair : txn_box.paused_pieces_) {
      auto &up_pause = pair.second;
      verify(up_pause);
      up_pause->Set(1);
    }
    txn_box.paused_pieces_.clear();
  } else {
    auto &up_pause = txn_box.paused_pieces_[inn_id];
    verify(up_pause);
    up_pause->Set(1);
    txn_box.paused_pieces_.erase(inn_id);
  }
}

void TxLogServer::reg_table(const std::string &name,
                            mdb::Table *tbl) {
  verify(mdb_txn_mgr_ != NULL);
  mdb_txn_mgr_->reg_table(name, tbl);
  if (name == TPCC_TB_ORDER) {
    mdb::Schema *schema = new mdb::Schema();
    const mdb::Schema *o_schema = tbl->schema();
    mdb::Schema::iterator it = o_schema->begin();
    for (; it != o_schema->end(); it++)
      if (it->indexed)
        if (it->name != "o_id")
          schema->add_column(it->name.c_str(), it->type, true);
    schema->add_column("o_c_id", Value::I32, true);
    schema->add_column("o_id", Value::I32, false);
    mdb_txn_mgr_->reg_table(TPCC_TB_ORDER_C_ID_SECONDARY,
                            new mdb::SortedTable(name, schema));
  }
}

void TxLogServer::DestroyExecutor(txnid_t txn_id) {
  Log_debug("destroy tid %ld\n", txn_id);
  auto it = executors_.find(txn_id);
  verify(it != executors_.end());
  auto exec = it->second;
  executors_.erase(it);
  delete exec;
}

void TxLogServer::Pause() {
  commo_->Pause();
};

void TxLogServer::Resume() {
  commo_->Resume();
};

void TxLogServer::TriggerUpgradeEpoch() {
  if (site_id_ == 0) {
    auto t_now = std::time(nullptr);
    auto d = std::difftime(t_now, last_upgrade_time_);
    if (d < EPOCH_DURATION || in_upgrade_epoch_) {
      return;
    }
    last_upgrade_time_ = t_now;
    in_upgrade_epoch_ = true;
    epoch_t epoch = epoch_mgr_.curr_epoch_;
    commo()->SendUpgradeEpoch(epoch,
                              std::bind(&TxLogServer::UpgradeEpochAck,
                                        this,
                                        std::placeholders::_1,
                                        std::placeholders::_2,
                                        std::placeholders::_3));
  }
}

void TxLogServer::UpgradeEpochAck(parid_t par_id,
                                  siteid_t site_id,
                                  int32_t res) {
  auto parids = Config::GetConfig()->GetAllPartitionIds();
  epoch_replies_[par_id][site_id] = res;
  if (epoch_replies_.size() < parids.size()) {
    return;
  }
  for (auto &pair: epoch_replies_) {
    auto par_id = pair.first;
    auto par_size = Config::GetConfig()->GetPartitionSize(par_id);
    verify(epoch_replies_[par_id].size() <= par_size);
    if (epoch_replies_[par_id].size() != par_size) {
      return;
    }
  }

  epoch_t smallest_inactive = 0xFFFFFFFF;
  for (auto &pair1 : epoch_replies_) {
    for (auto &pair2 : pair1.second) {
      if (smallest_inactive > pair2.second) {
        smallest_inactive = pair2.second;
      }
    }
  }
  in_upgrade_epoch_ = false;
  epoch_replies_.clear();
  int x = 5;
  if (smallest_inactive >= x) {
    epoch_t epoch_to_truncate = smallest_inactive - x;
    if (epoch_to_truncate >= epoch_mgr_.oldest_active_) {
      Log_info("truncate epoch %d", epoch_to_truncate);
      commo()->SendTruncateEpoch(epoch_to_truncate);
    }
  }
}

int32_t TxLogServer::OnUpgradeEpoch(uint32_t old_epoch) {
  epoch_mgr_.GrowActive();
  epoch_mgr_.GrowBuffer();
  return epoch_mgr_.CheckBufferInactive();
}

// below are about CURP

// [CURP] TODO: rm client_id & cmd_id_in_client since they already have a copy inside cmd
void TxLogServer::OnCurpDispatch(const int32_t& client_id,
                                  const int32_t& cmd_id_in_client,
                                  const shared_ptr<Marshallable>& cmd,
                                  bool_t* accepted,
                                  ver_t* ver,
                                  value_t* result,
                                  int32_t* finish_countdown,
                                  int32_t* key_hotness,
                                  siteid_t* coo_id,
                                  const function<void()> &cb) {
  // // used for debug
  // *accepted = false;
  // *ver = 0;
  // *result = 0;
  // *coo_id = 0;
  
  std::lock_guard<std::recursive_mutex> lock(mtx_);

  if (loc_id_ == 0) {
    if (SimpleRWCommand::GetCurrentMsTime() > last_print_structure_size_time_ + 10 * 1000) {
      if (abs(first_print_structure_size_time_) < 1e-5) {
        first_print_structure_size_time_ = SimpleRWCommand::GetCurrentMsTime();
      }
      last_print_structure_size_time_ = SimpleRWCommand::GetCurrentMsTime();
      PrintStructureSize();
    }
  }

  // Config *cfg = Config::GetConfig();
  // Log_info("[CURP] finish_countdown = %d", Config::GetConfig()->curp_finish_countdown_);
  // Log_info("[CURP] curp_fastpath_timeout_ = %d", Config::GetConfig()->curp_fastpath_timeout_);
  // Log_info("[CURP] curp_wait_commit_timeout_ = %d", Config::GetConfig()->curp_wait_commit_timeout_);
  // Log_info("[CURP] curp_instance_commit_timeout_ = %d", Config::GetConfig()->curp_instance_commit_timeout_);

  shared_ptr<SimpleRWCommand> parsed_cmd_ = make_shared<SimpleRWCommand>(cmd);
#ifdef CURP_FULL_LOG_DEBUG
  Log_info("[CURP] loc=%d, OnCurpDispatch of cmd<%d, %d>%s", loc_id_, client_id, cmd_id_in_client, parsed_cmd_->cmd_to_string().c_str());
#endif
  key_t key = dynamic_pointer_cast<SimpleRWCommand>(parsed_cmd_)->key_;
  // Log_info("[CURP] OnCurpPoorDispatch loc_id_ %d cmd (%d, %d) key=%d", loc_id_, client_id, cmd_id_in_client, key);

  struct timeval tp;
  gettimeofday(&tp, NULL);
  // double sent_time = ((VecPieceData*)(dynamic_pointer_cast<TpcCommitCommand>(cmd)->cmd_.get()))->time_sent_from_client_;
  double sent_time = (dynamic_pointer_cast<VecPieceData>(cmd))->time_sent_from_client_;
  double current_time = tp.tv_sec * 1000 + tp.tv_usec / 1000.0;
  cli2svr_dispatch.append(current_time - sent_time);

  if (curp_log_cols_[key] == nullptr)
    curp_log_cols_[key] = make_shared<CurpPlusDataCol>(this, key);
  shared_ptr<CurpPlusData> next_instance = curp_log_cols_[key]->NextInstance();
  *finish_countdown = finish_countdown_[key];
  if ((next_instance->status_ == CurpPlusData::CurpPlusStatus::INIT 
    || parsed_cmd_->type_ == RW_BENCHMARK_FINISH && next_instance->type_ == RW_BENCHMARK_FINISH
    || cmd_id_in_client != -1 && SimpleRWCommand::GetCmdID(next_instance->GetCmd()) == make_pair(client_id, cmd_id_in_client)
    )
      && finish_countdown_[key] == 0) {
    next_instance->status_ = CurpPlusData::CurpPlusStatus::PREACCEPT;
    next_instance->max_seen_ballot_ = 0;
    next_instance->UpdateCmd(cmd);
    if (parsed_cmd_->type_ == RW_BENCHMARK_R_TXN) {
      if (curp_log_cols_[key]->RecentExecutedInstance() == nullptr)
        *result = 0;
      else
        *result = curp_log_cols_[key]->RecentExecutedInstance()->value_;
    } else {
      *result = 1;
    }
    *accepted = true;
    *ver = curp_log_cols_[key]->NextVersion();
#ifdef CURP_REPEAR_COMMIT_DEBUG
    Log_info("[CURP] loc=%d OnCurpDispatch PreAccept [%s] since position(%d, %d) has status %d finish_countdown_[%d]=%d", loc_id_, parsed_cmd_->cmd_to_string().c_str(), key, curp_log_cols_[key]->NextVersion(), next_instance->status_, key, finish_countdown_[key]);
#endif
  } else {
#ifdef CURP_FULL_LOG_DEBUG
    Log_info("[CURP] loc=%d OnCurpDispatch Reject [%s] since position(%d, %d) has status %d finish_countdown_[%d]=%d", loc_id_, parsed_cmd_->cmd_to_string().c_str(), key, curp_log_cols_[key]->NextVersion(), next_instance->status_, key, finish_countdown_[key]);
#endif
    // Log_info("[CURP] loc=%d OnCurpDispatch Reject [%s] since position(%d, %d) has status %d [%s] finish_countdown_[%d]=%d",
      // loc_id_, parsed_cmd_->cmd_to_string().c_str(), key, curp_log_cols_[key]->NextVersion(), next_instance->status_, SimpleRWCommand(next_instance->GetCmd()).cmd_to_string().c_str(), key, finish_countdown_[key]);
    *accepted = false;
    *ver = -1;
    *result = -1;
  }
  *key_hotness = 0; // [CURP] TODO: this need to be discard
  *coo_id = 0;
  verify(curp_log_cols_[key]->logs_[curp_log_cols_[key]->NextVersion()] != nullptr);
#ifdef CURP_FULL_LOG_DEBUG
  Log_info("[CURP] About to CurpForwardResultToCoordinator, accepted=%d k=%d ver=%d result=%d", *accepted, key, *ver, *result);
#endif
  // Log_info("[CURP] loc=%d About to CurpForwardResultToCoordinator, accepted=%d k=%d ver=%d result=%d [%s]", loc_id_, *accepted, key, *ver, *result, SimpleRWCommand(cmd).cmd_to_string().c_str());
  bool_t accepted_cp = *accepted;
  ver_t ver_cp = *ver;
  shared_ptr<Marshallable> cmd_cp = cmd;
  WAN_WAIT;
  cb();
  if (loc_id_ == 0)
    OnCurpForward(accepted_cp, ver_cp, cmd_cp);
  else
    shared_ptr<IntEvent> sq_quorum = commo()->CurpForwardResultToCoordinator(partition_id_, accepted_cp, ver_cp, cmd_cp);
}

// Ze: This function is discard for efficiency consideration
void TxLogServer::OnCurpWaitCommit(const int32_t& client_id,
                                    const int32_t& cmd_id_in_client,
                                    bool_t* committed,
                                    value_t* commit_result,
                                    const function<void()> &cb) {
  verify(0);
//   std::lock_guard<std::recursive_mutex> lock(mtx_);
//   pair<int32_t, int32_t> cmd_id = make_pair(client_id, cmd_id_in_client);
//   if (executed_results_[cmd_id] == nullptr)
//     executed_results_[cmd_id] = make_shared<CommitNotification>();
//   executed_results_[cmd_id]->committed_ = committed;
//   executed_results_[cmd_id]->commit_result_ = commit_result;
//   executed_results_[cmd_id]->commit_callback_ = cb;
//   executed_results_[cmd_id]->client_stored_ = true;
//   executed_results_[cmd_id]->receive_time_ = SimpleRWCommand::GetCurrentMsTime();
//   commit_timeout_list_.push_back(executed_results_[cmd_id]);
// #ifdef CURP_CONFLICT_DEBUG
//   Log_info("[CURP] Client Stored commit callback for cmd<%d, %d>", client_id, cmd_id_in_client);
// #endif
//   if (executed_results_[cmd_id]->coordinator_stored_ && !executed_results_[cmd_id]->coordinator_replied_) {
//    *committed = executed_results_[cmd_id]->coordinator_commit_result_;
//    executed_results_[cmd_id]->coordinator_replied_ = true;
// #ifdef CURP_CONFLICT_DEBUG
//     Log_info("[CURP] Client Triggered commit callback for cmd<%d, %d>", client_id, cmd_id_in_client);
// #endif
//     WAN_WAIT;
//     cb();
//   }
//   Reactor::CreateSpEvent<TimeoutEvent>(Config::GetConfig()->curp_wait_commit_timeout_ * 1000)->Wait();
//   if (!executed_results_[cmd_id]->coordinator_replied_) {
//     curp_wait_commit_timeout_count_++;
// #ifdef CURP_FULL_LOG_DEBUG
//     Log_info("[CURP] cmd<%d, %d> WaitCommitTimeout, about to original protocol", cmd_id.first, cmd_id.second);
// #endif
//     *executed_results_[cmd_id]->committed_ = false;
//     *executed_results_[cmd_id]->commit_result_ = 0;
//     executed_results_[cmd_id]->commit_callback_();
//     executed_results_[cmd_id]->coordinator_replied_ = true;
//     // original_protocol_submit_count_++;
//   }
}

void TxLogServer::OnCurpForward(const bool_t& accepted,
                                const ver_t& ver,
                                const shared_ptr<Marshallable>& cmd) {
  std::lock_guard<std::recursive_mutex> lock(mtx_);
  key_t key = SimpleRWCommand::GetKey(cmd);
#ifdef CURP_CONFLICT_DEBUG
  Log_info("[CURP] Received Curp Forward of cmd<%d, %d> at pos(%d, %d)", SimpleRWCommand::GetCmdID(cmd).first, SimpleRWCommand::GetCmdID(cmd).second, SimpleRWCommand::GetKey(cmd), ver);
#endif
  if (!accepted) {
    return;
  }
  shared_ptr<ResponseData> response_pack = nullptr;
  uint64_t slot_id = SimpleRWCommand::CombineInt32(key, ver);
  if (curp_response_storage_[slot_id] == nullptr) {
    curp_response_storage_[slot_id] = make_shared<ResponseData>();
    response_pack = curp_response_storage_[slot_id];
    response_pack->first_seen_time_ = SimpleRWCommand::GetCurrentMsTime();
    // response_pack->pos_of_this_pack = make_pair(key, ver);
  } else {
    response_pack = curp_response_storage_[slot_id];
  }
  response_pack->received_count_++;
  pair<int, int> accepted_and_max_accepted = response_pack->append_response(cmd);
  int accepted_num = accepted_and_max_accepted.first;
  int max_accepted_num = accepted_and_max_accepted.second;
  int par_id_ = 0; // TODO: change to real par_id_;
  int n_replica = Config::GetConfig()->GetPartitionSize(par_id_);

  // Log_info("[CURP] !!! %p site=%d done=%d accepted_num=%d max_accepted_num=%d", (void*)this, site_id_, response_pack->done_, accepted_num, max_accepted_num);

  double current_time = SimpleRWCommand::GetCurrentMsTime();
  double time_elapses = current_time - response_pack->first_seen_time_;

#ifdef CURP_CONFLICT_DEBUG
  Log_info("[CURP] Forward Judgement of cmd<%d, %d>: %d %d, time_elapses=%.3f [%s]", 
    SimpleRWCommand::GetCmdID(cmd).first, 
    SimpleRWCommand::GetCmdID(cmd).second, 
    max_accepted_num >= CurpFastQuorumSize(n_replica), 
    (time_elapses > Config::GetConfig()->curp_fastpath_timeout_ || max_accepted_num + (n_replica - response_pack->received_count_) < CurpFastQuorumSize(n_replica))
      && accepted_num >= CurpQuorumSize(n_replica),
    time_elapses,
    SimpleRWCommand(cmd).cmd_to_string().c_str()
  );
#endif
  // Log_info("[CURP] Forward Judgement of cmd<%d, %d> at pos (%d, %d): %d %d, time_elapses=%.3f [%s]", 
  //   SimpleRWCommand::GetCmdID(cmd).first, 
  //   SimpleRWCommand::GetCmdID(cmd).second, 
  //   key,
  //   ver,
  //   max_accepted_num >= CurpFastQuorumSize(n_replica), 
  //   (time_elapses > Config::GetConfig()->curp_fastpath_timeout_ || max_accepted_num + (n_replica - response_pack->received_count_) < CurpFastQuorumSize(n_replica))
  //     && accepted_num >= CurpQuorumSize(n_replica),
  //   time_elapses,
  //   SimpleRWCommand(cmd).cmd_to_string().c_str()
  // );
  if (max_accepted_num >= CurpFastQuorumSize(n_replica)) {
    // Branch 1
    if (response_pack->done_) return;
    response_pack->done_ = true;
    shared_ptr<Marshallable> max_cmd = response_pack->GetMaxCmd();
    // [CURP] TODO: verify(cmd == max_cmd)
#ifdef CURP_REPEAR_COMMIT_DEBUG
    Log_info("Commit at FORWARD ver=%d [%s]", ver, SimpleRWCommand(max_cmd).cmd_to_string().c_str());
#endif
    CurpCommit(ver, max_cmd);
    curp_fast_path_success_count_++;
  } else if ( (time_elapses > Config::GetConfig()->curp_fastpath_timeout_ || max_accepted_num + (n_replica - response_pack->received_count_) < CurpFastQuorumSize(n_replica))
      && accepted_num >= CurpQuorumSize(n_replica) ) {
    // branch 2
    if (time_elapses > Config::GetConfig()->curp_fastpath_timeout_)
      curp_fastpath_timeout_count_++;
    if (response_pack->done_) return;
    response_pack->done_ = true;
    shared_ptr<Marshallable> max_cmd = response_pack->GetMaxCmd();
    CurpAccept(ver, 1, max_cmd);
  } 
  // else if (time_elapses > Config::GetConfig()->curp_fastpath_timeout_ && response_pack->received_count_ >= CurpQuorumSize(n_replica)) {
  //   // Branch 3
  //   // do nothings
  //   // shared_ptr<Marshallable> max_cmd = response_pack->GetMaxCmd();
  //   // shared_ptr<SimpleRWCommand> parsed_cmd_ = make_shared<SimpleRWCommand>(cmd);
  //   // CurpPrepare(key, ver, 1);
  // } else if (response_pack->received_count_ < CurpQuorumSize(n_replica)) {
  //   // Branch 4
  //   // do nothing
  // } else {
  //   // Branch 5
  //   // [CURP] TODO: May enter branch 5 when satisfied accepted_num >= CurpQuorumSize(n_replica) or response_pack->received_count_ >= CurpQuorumSize(n_replica)
  //   // in CURP_FAST_PATH_TIMEOUT and got no replies afterwards, need to create a timeout event judge branch 2 and branch 3 once CURP_FAST_PATH_TIMEOUT
  //   // verify(0);
  // }
  if (response_pack->received_count_ == Config::GetConfig()->GetPartitionSize(par_id_))
    curp_response_storage_.erase(slot_id);
}

void TxLogServer::CurpPrepare(key_t key,
                              ver_t ver,
                              ballot_t ballot) {
#ifdef CURP_FULL_LOG_DEBUG
  Log_info("[CURP] loc=%d CurpPrepare(k=%d, ver=%d, ballot=%d)", loc_id_, key, ver, ballot);
#endif
#ifdef CURP_REPEAR_COMMIT_DEBUG
  Log_info("[CURP] loc=%d CurpPrepare(k=%d, ver=%d, ballot=%d)", loc_id_, key, ver, ballot);
#endif
  shared_ptr<CurpPrepareQuorumEvent> e = commo()->CurpBroadcastPrepare(partition_id_, key, ver, ballot, loc_id_);
  e->Wait();
  shared_ptr<CurpPlusData> log = GetOrCreateCurpLog(key, ver);
  log->max_seen_ballot_ = max(log->max_seen_ballot_, e->GetMaxSeenBallot());
#ifdef CURP_REPEAR_COMMIT_DEBUG
  Log_info("[CURP] loc=%d CurpPrepare(k=%d, ver=%d, ballot=%d) Branch 1: CommitYes %d Branch 2: FastAcceptYes %d && %d >= %d Branch 3: %d Branch 4: %d >= %d",
    loc_id_, key, ver, ballot, e->CommitYes(), e->Yes(), e->max_fast_accept_count_, CurpSmallQuorumSize(e->n_total_), e->AcceptYes(), e->n_voted_yes_, e->quorum_);
#endif
  if (e->CommitYes()) {
#ifdef CURP_FULL_LOG_DEBUG
    Log_info("[CURP] loc=%d CurpPrepare(k=%d, ver=%d, ballot=%d) Branch 1: CommitYes", loc_id_, key, ver, ballot);
#endif
    // Log_info("[CURP] loc=%d CurpPrepare(k=%d, ver=%d, ballot=%d) Branch 1: CommitYes", loc_id_, key, ver, ballot);
#ifdef CURP_REPEAR_COMMIT_DEBUG
    Log_info("Commit at PREPARE ver=%d [%s]", ver, SimpleRWCommand(e->GetCommittedCmd()).cmd_to_string().c_str());
#endif
    CurpCommit(ver, e->GetCommittedCmd());
  } else if (e->FastAcceptYes()) {
#ifdef CURP_FULL_LOG_DEBUG
    Log_info("[CURP] loc=%d CurpPrepare(k=%d, ver=%d, ballot=%d) Branch 2: FastAcceptYes", loc_id_, key, ver, ballot);
#endif
    // Log_info("[CURP] loc=%d CurpPrepare(k=%d, ver=%d, ballot=%d) Branch 2: FastAcceptYes", loc_id_, key, ver, ballot);
    CurpAccept(ver, ballot, e->GetFastAcceptedCmd());
  } else if (e->AcceptYes()) {
#ifdef CURP_FULL_LOG_DEBUG
    Log_info("[CURP] loc=%d CurpPrepare(k=%d, ver=%d, ballot=%d) Branch 3: AcceptYes", loc_id_, key, ver, ballot);
#endif
    // Log_info("[CURP] loc=%d CurpPrepare(k=%d, ver=%d, ballot=%d) Branch 3: AcceptYes", loc_id_, key, ver, ballot);
    CurpAccept(ver, ballot, log->GetCmd());
  } else if (e->Yes()) {
#ifdef CURP_FULL_LOG_DEBUG
    Log_info("[CURP] loc=%d CurpPrepare(k=%d, ver=%d, ballot=%d) Branch 4: Yes", loc_id_, key, ver, ballot);
#endif
    // Log_info("[CURP] loc=%d CurpPrepare(k=%d, ver=%d, ballot=%d) Branch 4: Yes", loc_id_, key, ver, ballot);
    // [CURP] TODO: optimize: Accept self value(cmd) if not nullptr
    CurpAccept(ver, ballot, MakeNoOpCmd(partition_id_, key));
  }
  verify(curp_log_cols_[key]->logs_[ver] != nullptr);
}

void TxLogServer::CurpAccept(ver_t ver,
                              ballot_t ballot,
                              const shared_ptr<Marshallable>& cmd) {
  shared_ptr<SimpleRWCommand> parsed_cmd_ = make_shared<SimpleRWCommand>(cmd);
#ifdef CURP_FULL_LOG_DEBUG
  Log_info("[CURP] loc=%d CurpAccept(key=%d, ver=%d, ballot=%d, cmd<%d, %d>%s)", loc_id_, parsed_cmd_->key_, ver, ballot, parsed_cmd_->cmd_id_.first, parsed_cmd_->cmd_id_.second, parsed_cmd_->cmd_to_string().c_str());
#endif
  shared_ptr<CurpAcceptQuorumEvent> e = commo()->CurpBroadcastAccept(partition_id_, ver, ballot, cmd);
  e->Wait();
  if (e->Yes()) {
#ifdef CURP_FULL_LOG_DEBUG
    Log_info("[CURP] loc=%d CurpAccept(key=%d, ver=%d, ballot=%d, cmd<%d, %d>%s) Success about to commit", loc_id_, parsed_cmd_->key_, ver, ballot, parsed_cmd_->cmd_id_.first, parsed_cmd_->cmd_id_.second, parsed_cmd_->cmd_to_string().c_str());
#endif
#ifdef CURP_REPEAR_COMMIT_DEBUG
    Log_info("Commit at ACCEPT ver=%d [%s]", ver, SimpleRWCommand(cmd).cmd_to_string().c_str());
#endif
    CurpCommit(ver, cmd);
  }
}

void TxLogServer::CurpCommit(ver_t ver,
                            shared_ptr<Marshallable> cmd) {
  // Ze: I think commit same value several times is unavoidble since prepare and onforward will commit concurrently.
  if (ver < curp_log_cols_[SimpleRWCommand::GetKey(cmd)]->NextVersion()) {
    SimpleRWCommand cur_cmd = SimpleRWCommand(cmd);
    key_t key = cur_cmd.key_;
    SimpleRWCommand last_commit_cmd = SimpleRWCommand(curp_log_cols_[key]->logs_[ver]->GetCmd());
    // verify(cur_cmd.same_as(last_commit_cmd)); // TODO: add back
    curp_double_commit_count_++;
    return;
  }
#ifdef CURP_CONFLICT_DEBUG
  shared_ptr<vector<shared_ptr<SimpleCommand>>> sp_vec_piece{nullptr};
  if (cmd->kind_ == MarshallDeputy::CMD_NOOP) {
    // Commit No-Op, do nothing
  } else if (cmd->kind_ == MarshallDeputy::CMD_TPC_COMMIT) {
    shared_ptr<TpcCommitCommand> tpc_cmd = dynamic_pointer_cast<TpcCommitCommand>(cmd);
    VecPieceData *cmd_cast = (VecPieceData*)(tpc_cmd->cmd_.get());
    sp_vec_piece = cmd_cast->sp_vec_piece_data_;
  } else if (cmd->kind_ == MarshallDeputy::CMD_VEC_PIECE) {
    shared_ptr<VecPieceData> cmd_cast = dynamic_pointer_cast<VecPieceData>(cmd);
    sp_vec_piece = cmd_cast->sp_vec_piece_data_;
  } else {
    verify(0);
  }
  if (cmd->kind_ == MarshallDeputy::CMD_NOOP) {
    Log_info("[CURP] [CurpCommit] svr %d Commit No-Op pos(%d, %d)", loc_id_, SimpleRWCommand::GetKey(cmd), ver);
  } else {
    shared_ptr<TxPieceData> vector0 = *(sp_vec_piece->begin());
    Log_info("[CURP] [CurpCommit] svr %d Commit cmd<%d, %d> pos(%d, %d)", loc_id_, vector0->client_id_, vector0->cmd_id_in_client_, SimpleRWCommand::GetKey(cmd), ver);
  }
#endif
  OnCurpCommit(ver, cmd);
  commo()->CurpBroadcastCommit(partition_id_, ver, cmd, loc_id_);
}

void TxLogServer::OnCurpPrepare(const key_t& key,
                                const ver_t& ver,
                                const ballot_t& ballot,
                                bool_t* accepted,
                                int* status,
                                ballot_t* last_accepted_ballot,
                                MarshallDeputy* md_cmd,
                                const function<void()> &cb) {
  std::lock_guard<std::recursive_mutex> lock(mtx_);
#ifdef CURP_FULL_LOG_DEBUG
  Log_info("[CURP] loc=%d OnCurpPrepare(key=%d, ver=%d, ballot=%d)", loc_id_, key, ver, ballot);
#endif
  shared_ptr<CurpPlusData> log = GetOrCreateCurpLog(key, ver);
  verify(curp_log_cols_[key]->logs_[ver] != nullptr);
  if (curp_log_cols_[key]->logs_[ver]->status_ < CurpPlusData::COMMITTED && ballot > log->max_seen_ballot_) {
#ifdef CURP_REPEAR_COMMIT_DEBUG
    Log_info("[CURP] loc=%d OnCurpPrepare(key=%d, ver=%d, ballot=%d) > local_ballot=%d at %p, success, ret status=%d", loc_id_, key, ver, ballot, log->max_seen_ballot_, (void*)log.get(), log->status_);
#endif
    // Log_info("[CURP] loc=%d OnCurpPrepare(key=%d, ver=%d, ballot=%d) > local_ballot=%d, success, ret status=%d", loc_id_, key, ver, ballot, log->max_seen_ballot_, log->status_);
    log->max_seen_ballot_ = ballot;
#ifdef CURP_REPEAR_COMMIT_DEBUG
    Log_info("[CURP] loc=%d OnCurpPrepare(key=%d, ver=%d, ballot=%d) > local_ballot changed to %d", loc_id_, key, ver, ballot, log->max_seen_ballot_);
#endif
    // log->status_ = CurpPlusData::CurpPlusStatus::PREPARED;
    *accepted = true;
  } else {
#ifdef CURP_REPEAR_COMMIT_DEBUG
    Log_info("[CURP] loc=%d OnCurpPrepare(key=%d, ver=%d, ballot=%d) <= local_ballot=%d at %p, fail, ret status=%d", loc_id_, key, ver, ballot, log->max_seen_ballot_, (void*)log.get(), log->status_);
#endif
    // Log_info("[CURP] loc=%d OnCurpPrepare(key=%d, ver=%d, ballot=%d) <= local_ballot=%d, fail, ret status=%d", loc_id_, key, ver, ballot, log->max_seen_ballot_, log->status_);
    *accepted = false;
  }
  *status = log->status_;
  *last_accepted_ballot = log->last_accepted_ballot_;
  md_cmd->SetMarshallable(log->GetCmd());
  WAN_WAIT;
  cb();
}

void TxLogServer::OnCurpAccept(const ver_t& ver,
                                const ballot_t& ballot,
                                const shared_ptr<Marshallable>& cmd,
                                bool_t* accepted,
                                ballot_t* seen_ballot,
                                const function<void()> &cb) {
  std::lock_guard<std::recursive_mutex> lock(mtx_);
  shared_ptr<SimpleRWCommand> parsed_cmd_ = make_shared<SimpleRWCommand>(cmd);
#ifdef CURP_FULL_LOG_DEBUG
  Log_info("[CURP] loc=%d OnCurpAccept(key=%d, ver=%d, ballot=%d, cmd=[%s])", loc_id_, parsed_cmd_->key_, ver, ballot, parsed_cmd_->cmd_to_string().c_str());
#endif
  shared_ptr<CurpPlusData> log = GetOrCreateCurpLog(parsed_cmd_->key_, ver);
  if (curp_log_cols_[parsed_cmd_->key_]->logs_[ver]->status_ < CurpPlusData::COMMITTED && ballot >= log->max_seen_ballot_) {
#ifdef CURP_REPEAR_COMMIT_DEBUG
    Log_info("[CURP] loc=%d OnCurpAccept(key=%d, ver=%d, ballot=%d, cmd=[%s]) >= local_ballot=%d at %p, success", loc_id_, parsed_cmd_->key_, ver, ballot, parsed_cmd_->cmd_to_string().c_str(), log->max_seen_ballot_, (void*)log.get());
#endif
    log->UpdateCmd(cmd);
    log->max_seen_ballot_ = ballot;
    log->last_accepted_ballot_ = ballot;
    log->status_ = CurpPlusData::CurpPlusStatus::ACCEPTED;
    *accepted = true;
  } else {
#ifdef CURP_REPEAR_COMMIT_DEBUG
    Log_info("[CURP] loc=%d OnCurpAccept(key=%d, ver=%d, ballot=%d, cmd=[%s]) < local_ballot=%d at %p, fail", loc_id_, parsed_cmd_->key_, ver, ballot, parsed_cmd_->cmd_to_string().c_str(), log->max_seen_ballot_, (void*)log.get());
#endif
    *accepted = false;
  }
  *seen_ballot = log->max_seen_ballot_;
  WAN_WAIT;
  cb();
}

void TxLogServer::OnCurpCommit(const ver_t& ver,
                              const shared_ptr<Marshallable>& cmd) {
#ifdef CURP_FULL_LOG_DEBUG
  Log_info("[CURP] loc=%d OnCurpCommit ver=%d cmd<%d, %d>[%s]", loc_id_, ver, SimpleRWCommand::GetCmdID(cmd).first, SimpleRWCommand::GetCmdID(cmd).second, SimpleRWCommand(cmd).cmd_to_string().c_str());
#endif  
  // Log_info("[CURP] loc=%d OnCurpCommit ver=%d cmd<%d, %d>[%s]", loc_id_, ver, SimpleRWCommand::GetCmdID(cmd).first, SimpleRWCommand::GetCmdID(cmd).second, SimpleRWCommand(cmd).cmd_to_string().c_str());
  std::lock_guard<std::recursive_mutex> lock(mtx_);
  key_t key = SimpleRWCommand::GetKey(cmd);
  shared_ptr<CurpPlusData> instance = GetOrCreateCurpLog(key, ver);
  shared_ptr<SimpleRWCommand> parsed_cmd = make_shared<SimpleRWCommand>(instance->GetCmd()); 
  
  if (instance->status_ == CurpPlusData::CurpPlusStatus::COMMITTED || instance->status_ == CurpPlusData::CurpPlusStatus::EXECUTED)
    return;

  instance->UpdateCmd(cmd);
  instance->status_ = CurpPlusData::CurpPlusStatus::COMMITTED;

  struct timeval tp;
  gettimeofday(&tp, NULL);
  double sent_time = (dynamic_pointer_cast<VecPieceData>(instance->GetCmd()))->time_sent_from_client_;
  double current_time = tp.tv_sec * 1000 + tp.tv_usec / 1000.0;
  cli2svr_commit.append(current_time - sent_time);

  // if (curp_in_applying_logs_) {
  //   return;
  // }
  // curp_in_applying_logs_ = true;

  if (curp_log_cols_[key]->NextVersion() == ver) {
    while (instance->status_ == CurpPlusData::CurpPlusStatus::COMMITTED) {
      curp_log_cols_[key]->Execute(ver);
      instance = curp_log_cols_[key]->NextInstance();
    }
  }

  // curp_in_applying_logs_ = false;
}

shared_ptr<Marshallable> MakeFinishCmd(parid_t par_id, int cmd_id, key_t key, value_t value) {
  map<int32_t, Value> m;
  m[0] = key;
  m[1] = value;
  shared_ptr<TxPieceData> txPieceData = make_shared<TxPieceData>();
  txPieceData->client_id_ = -2;
  txPieceData->cmd_id_in_client_ = -2;
  txPieceData->partition_id_ = par_id;
  txPieceData->input.insert(m);
  txPieceData->type_ = RW_BENCHMARK_FINISH;
  shared_ptr<vector<shared_ptr<TxPieceData>>> sp_vec_piece = make_shared<vector<shared_ptr<TxPieceData>>>();
  sp_vec_piece->push_back(txPieceData);
  shared_ptr<VecPieceData> vecPiece = make_shared<VecPieceData>();
  vecPiece->sp_vec_piece_data_ = sp_vec_piece;
  return vecPiece;
}

shared_ptr<Marshallable> MakeNoOpCmd(parid_t par_id, key_t key) {
  map<int32_t, Value> m;
  m[0] = key;
  m[1] = 0;
  shared_ptr<TxPieceData> txPieceData = make_shared<TxPieceData>();
  txPieceData->client_id_ = -1;
  txPieceData->cmd_id_in_client_ = -1;
  txPieceData->partition_id_ = par_id;
  txPieceData->input.insert(m);
  txPieceData->type_ = RW_BENCHMARK_NOOP;
  shared_ptr<vector<shared_ptr<TxPieceData>>> sp_vec_piece = make_shared<vector<shared_ptr<TxPieceData>>>();
  sp_vec_piece->push_back(txPieceData);
  shared_ptr<VecPieceData> vecPiece = make_shared<VecPieceData>();
  vecPiece->sp_vec_piece_data_ = sp_vec_piece;
  return vecPiece;
}

void CurpPlusData::UpdateCmd(const shared_ptr<Marshallable> &cmd) {
  if (cmd == nullptr) {
    cmd_ = MakeNoOpCmd(svr_->partition_id_);
    shared_ptr<SimpleRWCommand> parsed_cmd_ = make_shared<SimpleRWCommand>(cmd_);
    type_ = parsed_cmd_->type_;
    key_ = parsed_cmd_->key_;
    value_ = parsed_cmd_->value_;
  } else {
    cmd_ = cmd;
    shared_ptr<SimpleRWCommand> parsed_cmd_ = make_shared<SimpleRWCommand>(cmd_);
    type_ = parsed_cmd_->type_;
    key_ = parsed_cmd_->key_;
    value_ = parsed_cmd_->value_;
    svr_->curp_instance_commit_timeout_pool_.AddTimeoutInstance(shared_from_this());
    svr_->curp_log_cols_[key_]->CheckHoles(ver_);
  }
}

void CurpPlusDataCol::CheckHoles(ver_t ver) {
  for (int i = latest_executed_ver_ + 1; i < ver; i++) {
    svr_->curp_instance_commit_timeout_pool_.AddTimeoutInstance(GetOrCreate(i));
  }
}

shared_ptr<Marshallable> CurpPlusData::GetCmd() {
  return cmd_;
}

void TxLogServer::CurpPreSkipFastpath(shared_ptr<Marshallable> &cmd) {
  if (loc_id_ != 0) return;

#ifdef LATENCY_DEBUG
  cli2preskip_begin_.append(SimpleRWCommand::GetCommandMsTimeElaps(cmd));
#endif

  key_t key = SimpleRWCommand::GetKey(cmd);

#ifdef CURP_FULL_LOG_DEBUG
  Log_info("[CURP] CurpPreSkipFastpath for cmd<%d, %d> since curp_in_commit_finish_[%d] is True", SimpleRWCommand::GetCmdID(cmd).first, SimpleRWCommand::GetCmdID(cmd).second, key);
#endif
  if (curp_in_commit_finish_[key])
    return;
#ifdef CURP_FULL_LOG_DEBUG
  Log_info("[CURP] curp_in_commit_finish_[%d] turned True for cmd<%d, %d>", key, SimpleRWCommand::GetCmdID(cmd).first, SimpleRWCommand::GetCmdID(cmd).second);
#endif
  curp_in_commit_finish_[key] = true;

  int attemp = 0;
  while (finish_countdown_[key] == 0) {
#ifdef CURP_FULL_LOG_DEBUG
    Log_info("[CURP] Attempted to precommit FIN for cmd<%d, %d> key=%d at svr %d",
      SimpleRWCommand::GetCmdID(cmd).first, SimpleRWCommand::GetCmdID(cmd).second, key, loc_id_);
#endif
    attemp++;
    shared_ptr<Marshallable> fin = MakeFinishCmd(partition_id_, -1, key, Config::GetConfig()->curp_finish_countdown_);
    verify(SimpleRWCommand::GetKey(fin) == key);
    auto e = commo()->CurpBroadcastDispatch(fin);
    e->Wait();
  }
#ifdef LATENCY_DEBUG
  preskip_attemp_.append(attemp);
#endif
#ifdef CURP_FULL_LOG_DEBUG
  Log_info("[CURP] curp_in_commit_finish_[%d] turned False for cmd<%d, %d>", key, SimpleRWCommand::GetCmdID(cmd).first, SimpleRWCommand::GetCmdID(cmd).second);
#endif
  curp_in_commit_finish_[key] = false;

#ifdef LATENCY_DEBUG
  cli2preskip_end_.append(SimpleRWCommand::GetCommandMsTimeElaps(cmd));
#endif
}


void TxLogServer::CurpSkipFastpath(int32_t cmd_id, shared_ptr<Marshallable> &cmd) {
  // SimpleRWCommand parsed_cmd = SimpleRWCommand(cmd);
  // verify(0);
  // Log_info("CurpSkipFastpath");
#ifdef LATENCY_DEBUG
  cli2skip_begin_.append(SimpleRWCommand::GetCommandMsTimeElaps(cmd));
#endif
#ifdef CURP_FULL_LOG_DEBUG
  Log_info("[CURP-FIN] cmd<-1, %d> Try to skipfastpath", cmd_id);
#endif
  original_protocol_submit_count_++;
  key_t key = SimpleRWCommand::GetKey(cmd);
  if (finish_countdown_[key] == 0) {
    // Coroutine::CreateRun([this, key, cmd_id]() { 
      for (int loop_count = 0, wait_time = 10; finish_countdown_[key] == 0; loop_count++, wait_time *= 2) {
        // Log_info("[CURP] Attempted to commit FIN for cmd<%d, %d> i.e. cmd<-1, %d> k=%d at svr %d at %.2f ms",
          // SimpleRWCommand::GetCmdID(cmd).first, SimpleRWCommand::GetCmdID(cmd).second, cmd_id, key, loc_id_, SimpleRWCommand::GetCurrentMsTime());
#ifdef CURP_FULL_LOG_DEBUG
        Log_info("[CURP] Attempted to commit FIN for cmd<%d, %d> i.e. cmd<-1, %d> key=%d at svr %d",
          SimpleRWCommand::GetCmdID(cmd).first, SimpleRWCommand::GetCmdID(cmd).second, cmd_id, key, loc_id_);
#endif
        if (loop_count > 8) {
          // Log_info("[CURP] REALLY Attempted to CommitFinish for cmd<%d, %d> i.e. cmd<-1, %d> k=%d at svr %d at %.2f ms",
          // SimpleRWCommand::GetCmdID(cmd).first, SimpleRWCommand::GetCmdID(cmd).second, cmd_id, key, loc_id_, SimpleRWCommand::GetCurrentMsTime());
          shared_ptr<Marshallable> fin = MakeFinishCmd(partition_id_, cmd_id, key, Config::GetConfig()->curp_finish_countdown_);
          verify(SimpleRWCommand::GetKey(fin) == key);
          auto e = commo()->CurpBroadcastDispatch(fin);
          e->Wait();
        } else {
          auto timeout_e = Reactor::CreateSpEvent<TimeoutEvent>(wait_time * 1000);
          timeout_e->Wait();
        }
      }
    // });
  }
#ifdef CURP_FULL_LOG_DEBUG
  Log_info("[CURP-FIN] Success skip fastpath for cmd<%d, %d> i.e. cmd<-1, %d> at svr %d, countdown decreased to %d",
    SimpleRWCommand::GetCmdID(cmd).first, SimpleRWCommand::GetCmdID(cmd).second, cmd_id, loc_id_, finish_countdown_[key] - 1);
#endif
  finish_countdown_[key]--;
#ifdef LATENCY_DEBUG
  cli2skip_end_.append(SimpleRWCommand::GetCommandMsTimeElaps(cmd));
#endif
}

void CurpCoordinatorCommitFinishTimeoutPool::TimeoutLoop() {
  // [CURP] Ze: I think it's not necessary if all the replies can be received. I the case of package loss, need to 
  // design carefully to avoid erase while traverse.
  // while (true) {
  //   Reactor::CreateSpEvent<TimeoutEvent>(10 * 1000)->Wait();
  //   // Log_info("CurpCoordinatorCommitFinishTimeoutPool::TimeoutLoop");
  //   for (unordered_map<int64_t, pair<key_t, shared_ptr<CurpDispatchQuorumEvent>>>::iterator it = wait_for_commit_events_pool_.begin(), cur_it; it != wait_for_commit_events_pool_.end(); ) {
  //     cur_it = it;
  //     it++;
  //     DealWith(cur_it->first);
  //   }
  // }
}

void CurpCoordinatorCommitFinishTimeoutPool::DealWith(uint64_t cmd_id) {
  // std::lock_guard<std::recursive_mutex> lock(sch_->curp_mtx_);
  key_t key = wait_for_commit_events_pool_[cmd_id].first;
  // Log_info("DealWith k=%d at %.2f ms", key, SimpleRWCommand::GetCurrentMsTime());
  shared_ptr<CurpDispatchQuorumEvent> e = wait_for_commit_events_pool_[cmd_id].second;
  if (e == nullptr) return;
  if (e->FastYes()) {
    // Log_info("CurpCoordinatorCommitFinish FastYes for cmd<%lld, %lld>", cmd_id >> 31, cmd_id & ((1ll << 31) - 1));
    shared_ptr<Marshallable> finish = MakeFinishCmd(sch_->partition_id_, -1, key, Config::GetConfig()->curp_finish_countdown_);
    // Log_info("Commit [%s] at %.2f ms", SimpleRWCommand(finish).cmd_to_string().c_str(), SimpleRWCommand::GetCurrentMsTime());
    sch_->OnCurpCommit(e->GetMax().ver_, finish);
    sch_->commo()->CurpBroadcastCommit(sch_->partition_id_, e->GetMax().ver_, finish, sch_->loc_id_);
    commit_finish_in_pool_.erase(wait_for_commit_events_pool_[cmd_id].first);
    wait_for_commit_events_pool_.erase(cmd_id);
  } else if (e->FastNo() || e->timeouted_) {
    // Log_info("CurpCoordinatorCommitFinish FastNo or Timeout for cmd<%lld, %lld>", cmd_id >> 31, cmd_id & ((1ll << 31) - 1));
    shared_ptr<Marshallable> finish = MakeFinishCmd(sch_->partition_id_, -1, key, Config::GetConfig()->curp_finish_countdown_);
    // int attempt_cnt = 0;
    double send_gap_ms = 10;
    while (sch_->finish_countdown_[key] == 0) {
      // Log_info("key=%d, attemp_count=%d", key, ++attempt_cnt);
      shared_ptr<CurpDispatchQuorumEvent> new_e = sch_->commo()->CurpBroadcastDispatch(finish);
      new_e->Wait();
      if (new_e->FastYes()) {
        sch_->OnCurpCommit(new_e->GetMax().ver_, finish);
        sch_->commo()->CurpBroadcastCommit(sch_->partition_id_, new_e->GetMax().ver_, finish, sch_->loc_id_);
      }
      Reactor::CreateSpEvent<TimeoutEvent>(send_gap_ms * 1000)->Wait();
      send_gap_ms *= 2;
    }
    commit_finish_in_pool_.erase(wait_for_commit_events_pool_[cmd_id].first);
    wait_for_commit_events_pool_.erase(cmd_id);
  }
}

uint64_t TxLogServer::CurpAttemptCommitFinish(shared_ptr<Marshallable> &cmd) {
  // only server on loc_0, the coordinator, takes charge of this
  // if (loc_id_ != 0)
  //   return 0;
#ifdef CURP_AVOID_CurpSkipFastpath_DEBUG
  Log_info("loc=%d CurpAttemptCommitFinish for [%s] at %.2f", loc_id_, SimpleRWCommand(cmd).cmd_to_string().c_str(), SimpleRWCommand::GetCurrentMsTime());
#endif
  if (!curp_coordinator_commit_finish_timeout_pool_.timeout_loop_started_) {
    curp_coordinator_commit_finish_timeout_pool_.timeout_loop_started_ = true;
    Coroutine::CreateRun([this]() { 
      curp_coordinator_commit_finish_timeout_pool_.TimeoutLoop();
    });
  }
  key_t key = SimpleRWCommand::GetKey(cmd);
  pair<int32_t, int32_t> cmd_id = SimpleRWCommand::GetCmdID(cmd);
  // if (false) {
  if (finish_countdown_[key] == 0 && curp_coordinator_commit_finish_timeout_pool_.commit_finish_in_pool_.find(key) == curp_coordinator_commit_finish_timeout_pool_.commit_finish_in_pool_.end()) {
    curp_coordinator_commit_finish_timeout_pool_.commit_finish_in_pool_.insert(key);
    int n = Config::GetConfig()->GetPartitionSize(partition_id_);
    curp_coordinator_commit_finish_timeout_pool_.wait_for_commit_events_pool_[SimpleRWCommand::CombineInt32(cmd_id)]
     = make_pair(key, Reactor::CreateSpEvent<CurpDispatchQuorumEvent>(n, CurpQuorumSize(n)));
    return Config::GetConfig()->curp_finish_countdown_;
  }
  else
    return 0; // means do not need to commit finish this time
}

void TxLogServer::CurpAttemptCommitFinishReply(pair<int32_t, int32_t> cmd_id,
                                                bool_t &finish_accept,
                                                uint64_t &finish_ver) {
  // if (loc_id_ != 0)
  //   return;
#ifdef CURP_AVOID_CurpSkipFastpath_DEBUG
  Log_info("CurpAttemptCommitFinishReply for cmd<%d, %d> at %.2f", cmd_id.first, cmd_id.second, SimpleRWCommand::GetCurrentMsTime());
#endif
  auto it = curp_coordinator_commit_finish_timeout_pool_.wait_for_commit_events_pool_.find(SimpleRWCommand::CombineInt32(cmd_id));
  if (it != curp_coordinator_commit_finish_timeout_pool_.wait_for_commit_events_pool_.end()) {
    it->second.second->FeedResponse(finish_accept, finish_ver, 0, 0, 0, 0);
    curp_coordinator_commit_finish_timeout_pool_.DealWith(SimpleRWCommand::CombineInt32(cmd_id));
  }
}

void TxLogServer::OnCurpAttemptCommitFinish(shared_ptr<Marshallable> &cmd,
                              const uint64_t& commit_finish,
                              bool_t* finish_accept,
                              uint64_t* finish_ver) {
  if (commit_finish == 0) {
    // commit_finish means no need to commit finish symbol this time
    *finish_accept = false;
    *finish_ver = 0;
    return;
  }
  key_t key = SimpleRWCommand::GetKey(cmd);
  if (curp_log_cols_[key] == nullptr)
    curp_log_cols_[key] = make_shared<CurpPlusDataCol>(this, key);
  shared_ptr<CurpPlusData> next_instance = curp_log_cols_[key]->NextInstance();
  if (next_instance->status_ == CurpPlusData::CurpPlusStatus::INIT && finish_countdown_[key] == 0) {
    next_instance->status_ = CurpPlusData::CurpPlusStatus::PREACCEPT;
    next_instance->max_seen_ballot_ = 0;
    shared_ptr<Marshallable> finish_cmd = MakeFinishCmd(partition_id_, -1, key, commit_finish);
    next_instance->UpdateCmd(finish_cmd);
    *finish_accept = true;
    *finish_ver = curp_log_cols_[key]->NextVersion();
  } else {
    *finish_accept = false;
    *finish_ver = 0;
  }
}

CurpPlusData::CurpPlusData(TxLogServer* svr, key_t key, ver_t ver, CurpPlusStatus status, const shared_ptr<Marshallable> &cmd, ballot_t ballot)
  : svr_(svr), key_(key), ver_(ver), status_(status), max_seen_ballot_(ballot) {
#ifdef CURP_INSTANCE_CREATED_ONLY_ONCE_CHECK
  verify(!svr_->assigned_[make_pair(key, ver)]);
  svr_->assigned_[make_pair(key, ver)] = true;
#endif
#ifdef CURP_FULL_LOG_DEBUG
  Log_info("[CURP] Create instance for pos(%d, %d)", key, ver);
#endif
  key_ = key;
  ver_ = ver;
  UpdateCmd(cmd);
}

shared_ptr<CurpPlusData> CurpPlusDataCol::GetOrCreate(ver_t ver) {
  if (logs_[ver] == nullptr) {
#ifdef CURP_INSTANCE_CREATED_ONLY_ONCE_CHECK
    verify(!svr_->assigned_[make_pair(key_, ver)]);
#endif
    logs_[ver] = make_shared<CurpPlusData>(svr_, key_, ver, CurpPlusData::CurpPlusStatus::INIT, nullptr, 0);
#ifdef CURP_INSTANCE_CREATED_ONLY_ONCE_CHECK
    svr_->assigned_[make_pair(key_, ver)] = true;
#endif
  }
  verify(logs_[ver] != nullptr);
  return logs_[ver];
}

void CurpPlusDataCol::Execute(ver_t ver) {
  verify(latest_executed_ver_ + 1 == ver);

  shared_ptr<CurpPlusData> instance = NextInstance();
  verify(instance->status_ == CurpPlusData::CurpPlusStatus::COMMITTED);

  if (instance->type_ == RW_BENCHMARK_NOOP) {
    // Do nothing
  } else if (instance->type_ == RW_BENCHMARK_FINISH) {
#ifdef CURP_FULL_LOG_DEBUG
    Log_info("[CURP] loc=%d finish_countdown_[%d] + %d = %d", svr_->loc_id_, key_, instance->value_, svr_->finish_countdown_[key_] + instance->value_);
#endif
    svr_->finish_countdown_count_++;
#ifdef CURP_FULL_LOG_DEBUG
    Log_info("[CURP-FIN] countdown increased to %d", svr_->finish_countdown_[key_] + instance->value_);
#endif
    svr_->finish_countdown_[key_] += instance->value_;
  } else {
    value_t result;
    if (instance->type_ == RW_BENCHMARK_R_TXN) {
      result = svr_->DBGet(instance->GetCmd());
    } else if (instance->type_ == RW_BENCHMARK_W_TXN) {
      result = svr_->DBPut(instance->GetCmd());
    } else {
      verify(0);
    }
//     pair<int32_t, int32_t> cmd_id = SimpleRWCommand::GetCmdID(instance->GetCmd());

//     // store execute result
//     if (svr_->executed_results_[cmd_id] == nullptr)
//       svr_->executed_results_[cmd_id] = make_shared<CommitNotification>();
//     svr_->executed_results_[cmd_id]->coordinator_commit_result_ = result;
//     svr_->executed_results_[cmd_id]->coordinator_stored_ = true;
// #ifdef CURP_FULL_LOG_DEBUG
//     Log_info("[CURP] Server Stored commit result for cmd<%d, %d>", cmd_id.first, cmd_id.second);
// #endif

//     // reply execute result if asked
//     if (svr_->executed_results_[cmd_id]->client_stored_ && !svr_->executed_results_[cmd_id]->coordinator_replied_) {
//       *svr_->executed_results_[cmd_id]->commit_result_ = svr_->executed_results_[cmd_id]->coordinator_commit_result_;
//       svr_->executed_results_[cmd_id]->coordinator_replied_ = true;
//       svr_->executed_results_[cmd_id]->commit_callback_();
// #ifdef CURP_FULL_LOG_DEBUG
//       Log_info("[CURP] Server Triggered commit callback for cmd<%d, %d>", cmd_id.first, cmd_id.second);
// #endif
//     } else { 
// #ifdef CURP_FULL_LOG_DEBUG
//       Log_info("[CURP] Server Fail to Trigger commit callback for cmd<%d, %d> for judgement stored=%d replied=%d", cmd_id.first, cmd_id.second, svr_->executed_results_[cmd_id]->client_stored_, svr_->executed_results_[cmd_id]->coordinator_replied_);
// #endif
//     }
  }
    
  instance->status_ = CurpPlusData::CurpPlusStatus::EXECUTED;

  latest_executed_ver_ = ver;

  // garbage collection
  pair<key_t, ver_t> to_erase_ = svr_->curp_executed_garbage_collection_[svr_->curp_executed_garbage_collection_pointer_];
  if (to_erase_.second > 0) // ver starts from 1
    svr_->curp_log_cols_[to_erase_.first]->logs_.erase(to_erase_.second);
  svr_->curp_executed_garbage_collection_[svr_->curp_executed_garbage_collection_pointer_] = make_pair(key_, ver);
  svr_->curp_executed_garbage_collection_pointer_++;
  if (svr_->curp_executed_garbage_collection_pointer_ >= 100000)
    svr_->curp_executed_garbage_collection_pointer_ = 0;
}

void CurpPlusDataCol::Print() {
  string str;
  for (auto log: logs_) {
    if (log.first == 0) continue;
    SimpleRWCommand parsed_cmd = SimpleRWCommand(log.second->GetCmd());
    str += "[" + to_string(log.first) + ":" + parsed_cmd.cmd_to_string() + "]";
  }
  Log_info("{key=%d, %s}", key_, str.c_str());
}

shared_ptr<CurpPlusData> TxLogServer::GetCurpLog(key_t key, ver_t ver) {
  if (curp_log_cols_.count(key)) {
    if (curp_log_cols_[key]->logs_.count(ver)) {
      return curp_log_cols_[key]->logs_[ver];
    } else {
      return nullptr;
    }
  } else {
    return nullptr;
  }
}

shared_ptr<CurpPlusData> TxLogServer::GetOrCreateCurpLog(key_t key, ver_t ver) {
  std::lock_guard<std::recursive_mutex> lock(mtx_);
  if (curp_log_cols_[key] == nullptr)
    curp_log_cols_[key] = make_shared<CurpPlusDataCol>(this, key);
  shared_ptr<CurpPlusDataCol> col = curp_log_cols_[key];
  shared_ptr<CurpPlusData> ret = col->GetOrCreate(ver);
  return ret;
}

UniqueCmdID TxLogServer::GetUniqueCmdID(shared_ptr<Marshallable> cmd) {
  shared_ptr<vector<shared_ptr<SimpleCommand>>> sp_vec_piece{nullptr};
  if (cmd->kind_ == MarshallDeputy::CMD_TPC_COMMIT) {
    shared_ptr<TpcCommitCommand> tpc_cmd = dynamic_pointer_cast<TpcCommitCommand>(cmd);
    VecPieceData *cmd_cast = (VecPieceData*)(tpc_cmd->cmd_.get());
    sp_vec_piece = cmd_cast->sp_vec_piece_data_;
  } else if (cmd->kind_ == MarshallDeputy::CMD_VEC_PIECE) {
    shared_ptr<VecPieceData> cmd_cast = dynamic_pointer_cast<VecPieceData>(cmd);
    sp_vec_piece = cmd_cast->sp_vec_piece_data_;
  } else {
    verify(0);
  }
  shared_ptr<TxPieceData> vector0 = *(sp_vec_piece->begin());
  shared_ptr<CmdData> casted_cmd = dynamic_pointer_cast<CmdData>(vector0);
  UniqueCmdID cmd_id;
  cmd_id.client_id_ = casted_cmd->client_id_;
  cmd_id.cmd_id_ = casted_cmd->cmd_id_in_client_;
  return cmd_id;
}

value_t TxLogServer::DBGet(const shared_ptr<Marshallable>& cmd) {
  shared_ptr<SimpleRWCommand> parsed_cmd_ = make_shared<SimpleRWCommand>(cmd);
  return kv_table_[parsed_cmd_->key_];
}

value_t TxLogServer::DBPut(const shared_ptr<Marshallable>& cmd) {
  shared_ptr<SimpleRWCommand> parsed_cmd_ = make_shared<SimpleRWCommand>(cmd);
  kv_table_[parsed_cmd_->key_] = parsed_cmd_->value_;
  return 1;
}

void CurpInstanceCommitTimeoutPool::TimeoutLoop() {
  while (true) {
    // Log_info("CurpInstanceCommitTimeoutPool start, wait for %d us", Config::GetConfig()->curp_instance_commit_timeout_ * 1000 + rand() % (Config::GetConfig()->curp_instance_commit_timeout_ * 1000));
    Reactor::CreateSpEvent<TimeoutEvent>(Config::GetConfig()->curp_instance_commit_timeout_ * 1000 + rand() % (Config::GetConfig()->curp_instance_commit_timeout_ * 1000))->Wait();
    // Reactor::CreateSpEvent<NeverEvent>()->Wait(Config::GetConfig()->curp_instance_commit_timeout_ * 1000 + rand() % (Config::GetConfig()->curp_instance_commit_timeout_ * 1000));
    // Reactor::CreateSpEvent<TimeoutEvent>(100000)->Wait();
    // Log_info("Finish Wait");
    for (set<pair<double, shared_ptr<CurpPlusData>>>::iterator it = curp_instance_commit_pool_.begin(), cur_it; it != curp_instance_commit_pool_.end(); ) {
      if (it->first < SimpleRWCommand::GetCurrentMsTime()) {
        if (it->second->status_ == CurpPlusData::CurpPlusStatus::COMMITTED || it->second->status_ == CurpPlusData::CurpPlusStatus::EXECUTED) {
          curp_instance_commit_in_pool_.erase(SimpleRWCommand::CombineInt32(it->second->GetPos()));
// #ifdef CURP_FULL_LOG_DEBUG
          // pair<int32_t, int32_t> cmd_id = SimpleRWCommand::GetCmdID(it->second->GetCmd());
          // Log_info("Instance Commit Timeout Branch 1: cmd<%d, %d> key=%d", cmd_id.first, cmd_id.second, SimpleRWCommand::GetKey(it->second->GetCmd()));
// #endif
          cur_it = it;
          it++;
          curp_instance_commit_pool_.erase(cur_it);
        } else {
          it->second->svr_->curp_instance_commit_timeout_trigger_prepare_count_++;
          it->second->PrepareInstance();
          AddTimeoutInstance(it->second, true);
// #ifdef CURP_FULL_LOG_DEBUG
          // pair<int32_t, int32_t> cmd_id = SimpleRWCommand::GetCmdID(it->second->GetCmd());
          // Log_info("Instance Commit Timeout Branch 2: cmd<%d, %d> key=%d", cmd_id.first, cmd_id.second, SimpleRWCommand::GetKey(it->second->GetCmd()));
// #endif
          cur_it = it;
          it++;
          curp_instance_commit_pool_.erase(cur_it);
        }
      } else {
        ++it;
        // Log_info("Branch 3");
      }
    }
  }
}
void CurpInstanceCommitTimeoutPool::AddTimeoutInstance(shared_ptr<CurpPlusData> instance, bool repeat_insert) {
  verify(instance->GetCmd() != nullptr);
  if (!timeout_loop_started_) {
    timeout_loop_started_ = true;
    Coroutine::CreateRun([this]() { 
      TimeoutLoop();
    });
  }
  if (instance->status_ == CurpPlusData::CurpPlusStatus::COMMITTED || instance->status_ == CurpPlusData::CurpPlusStatus::EXECUTED)
    return;
  double trigger_ms_timeout = Config::GetConfig()->curp_instance_commit_timeout_ + rand() % (Config::GetConfig()->curp_instance_commit_timeout_ * 1000) / 1000.0;
  if (repeat_insert) {
    curp_instance_commit_pool_.insert(make_pair(SimpleRWCommand::GetCurrentMsTime() + trigger_ms_timeout, instance));
    return;
  }
  pair<key_t, ver_t> pos = instance->GetPos();
  if (curp_instance_commit_in_pool_.count(SimpleRWCommand::CombineInt32(pos)) == 0) {
    curp_instance_commit_in_pool_.insert(SimpleRWCommand::CombineInt32(pos));
    curp_instance_commit_pool_.insert(make_pair(SimpleRWCommand::GetCurrentMsTime() + trigger_ms_timeout, instance));
  }
}

void CurpPlusData::PrepareInstance() {
  max_seen_ballot_++;
  if (max_seen_ballot_ < 2)
    max_seen_ballot_ = 2;
  svr_->CurpPrepare(key_, ver_, max_seen_ballot_);
}

void TxLogServer::PrintStructureSize() {
  Log_info("Time start from start  %.2f", last_print_structure_size_time_ - first_print_structure_size_time_);
  // curp_log_cols_
  int curp_log_cols_size_ = 0;
  for (unordered_map<key_t, shared_ptr<CurpPlusDataCol>>::iterator it = curp_log_cols_.begin(); it != curp_log_cols_.end(); it++) {
    curp_log_cols_size_ += it->second->logs_.size();
  }
  Log_info("[Structure Size] curp_log_cols_ %d", curp_log_cols_size_);
  // CurpInstanceCommitTimeoutPool
  Log_info("[Structure Size] curp_instance_commit_timeout_pool_ %d", curp_instance_commit_timeout_pool_.curp_instance_commit_in_pool_.size() + curp_instance_commit_timeout_pool_.curp_instance_commit_pool_.size());
  // CurpCoordinatorCommitFinishTimeoutPool
  Log_info("[Structure Size] curp_coordinator_commit_finish_timeout_pool_ %d", curp_coordinator_commit_finish_timeout_pool_.commit_finish_in_pool_.size() + curp_coordinator_commit_finish_timeout_pool_.wait_for_commit_events_pool_.size());
  // curp_response_storage_
  Log_info("[Structure Size] curp_response_storage_ %d", curp_response_storage_.size());
  // curp_in_commit_finish_
  Log_info("[Structure Size] curp_in_commit_finish_ %d", curp_in_commit_finish_.size());
  // executed_results_
  // Log_info("[Structure Size] executed_results_ %d", executed_results_.size());
  // commit_timeout_list_
  Log_info("[Structure Size] commit_timeout_list_ %d", commit_timeout_list_.size());
  // finish_countdown_
  Log_info("[Structure Size] finish_countdown_ %d", finish_countdown_.size());
  // kv_table_
  Log_info("[Structure Size] kv_table_ %d", kv_table_.size());
  // assigned_
#ifdef CURP_INSTANCE_CREATED_ONLY_ONCE_CHECK
  Log_info("[Structure Size] assigned_ %d", assigned_.size());
#endif
}

// below are about rule

void TxLogServer::OnRuleSpeculativeExecute(const shared_ptr<Marshallable>& cmd,
                    bool_t* accepted,
                    value_t* result,
                    bool_t* is_leader) {
#ifdef ZERO_OVERHEAD
  // if (rep_sched_->ConflictWithOriginalUnexecutedLog(cmd))
  //   Log_info("Conflict!");
  if (rep_sched_->witness_.push_back(cmd) && !rep_sched_->ConflictWithOriginalUnexecutedLog(cmd)) {
#else
  if (rep_sched_->witness_.push_back(cmd)) {
#endif
    // SimpleRWCommand parsed_cmd = SimpleRWCommand(cmd);
    // Log_info("Server %d OnRuleSpeculativeExecute <%d, %d> key %d", rep_sched_->loc_id_, parsed_cmd.cmd_id_.first, parsed_cmd.cmd_id_.second, parsed_cmd.key_);
    // Log_info("witness_.push_back server %d push cmd_id <%d, %d> %lld key %d success 1", loc_id_, parsed_cmd.cmd_id_.first, parsed_cmd.cmd_id_.second,
      // (long long)SimpleRWCommand::CombineInt32(parsed_cmd.cmd_id_.first, parsed_cmd.cmd_id_.second), parsed_cmd.key_);
    // verify(witness_.remove(cmd));
    *accepted = true;
    // [RULE] TODO: return speculative result
    *result = 0;
  } else {
    *accepted = false;
  }
  *is_leader = IsLeader();
}

void TxLogServer::OriginalPathUnexecutedCmdConflictPlaceHolder(const shared_ptr<Marshallable>& cmd) {
  if (Config::GetConfig()->tx_proto_ == MODE_RULE && SimpleRWCommand::NeedRecordConflictInOriginalPath(cmd))
    rep_sched_->witness_.push_back(cmd);
}

void TxLogServer::RuleWitnessGC(const shared_ptr<Marshallable>& cmd) {
  if (Config::GetConfig()->tx_proto_ == MODE_RULE)
    witness_.remove(cmd);
  // SimpleRWCommand parsed_cmd = SimpleRWCommand(cmd);
  // uint64_t cmd_id = SimpleRWCommand::CombineInt32(parsed_cmd.cmd_id_.first, parsed_cmd.cmd_id_.second);
  // Log_info("witness_.remove server %d remove cmd_id <%d, %d> %lld key %d success %d", loc_id_, parsed_cmd.cmd_id_.first, parsed_cmd.cmd_id_.second,
  //     (long long)SimpleRWCommand::CombineInt32(parsed_cmd.cmd_id_.first, parsed_cmd.cmd_id_.second), parsed_cmd.key_, witness_.remove(cmd));
  // Log_info("witness_.remove(cmd) %d", witness_.remove(cmd));
  // witness_.remove(cmd);
}


void RevoveryCandidates::push_back(uint64_t cmd_id, bool is_write) {
  candidates_[cmd_id] = make_pair(++maximal_, is_write);
  total_write_ += is_write;
}

bool RevoveryCandidates::remove(uint64_t cmd_id) {
  auto it = candidates_.find(cmd_id);
  if (it != candidates_.end()) {
    total_write_ -= it->second.second;
    candidates_.erase(cmd_id);
    return 1;
  } else {
    return 0;
  }
  // return candidates_.erase(cmd_id);
}

size_t RevoveryCandidates::size() {
  return candidates_.size();
}

int RevoveryCandidates::total_write() {
  return total_write_;
}

uint64_t RevoveryCandidates::id_of_candidate_to_recover() {
  uint64_t cmd_to_recover = -1;
  int minimal = INT_MAX;
  for (auto pair: candidates_) {
    if (pair.second.first < minimal) {
      cmd_to_recover = pair.first;
      minimal = pair.second.first;
    }
  }
  return cmd_to_recover;
}

bool Witness::push_back(const shared_ptr<Marshallable>& cmd) {
  SimpleRWCommand parsed_cmd = SimpleRWCommand(cmd);
  key_t key = parsed_cmd.key_;
  uint64_t cmd_id = SimpleRWCommand::CombineInt32(parsed_cmd.cmd_id_.first, parsed_cmd.cmd_id_.second);

#ifdef READ_NOT_CONFLICT_OPTIMIZATION
  // if (!(candidates_[key].size() == 0 || (candidates_[key].total_write() == 0 && parsed_cmd.IsRead())))
  //   Log_info("total_write %d parsed_cmd.IsRead() %d parsed_cmd.IsWrite() %d", candidates_[key].total_write(), parsed_cmd.IsRead(), parsed_cmd.IsWrite());
  if (candidates_[key].size() == 0 || (candidates_[key].total_write() == 0 && parsed_cmd.IsRead())) {
#endif
#ifndef READ_NOT_CONFLICT_OPTIMIZATION
  if (candidates_[key].size() == 0) {
#endif
    // not exist conflict
    candidates_[key].push_back(cmd_id, parsed_cmd.IsWrite());
#ifdef WITNESS_LOG_DEBUG
    witness_log_.push_back(WitnessLog(0, cmd, 1, witness_size_));
#endif
    witness_size_distribution_.mid_time_append(++witness_size_);
    return true;
  } else {
    // exist conflict, candidates_[key].size() >= 1
    if (belongs_to_leader_) {
      candidates_[key].push_back(cmd_id, parsed_cmd.IsWrite());
#ifdef WITNESS_LOG_DEBUG
      witness_log_.push_back(WitnessLog(0, cmd, 2, witness_size_));
#endif
      witness_size_distribution_.mid_time_append(++witness_size_);
    } else {
#ifdef WITNESS_LOG_DEBUG
      witness_log_.push_back(WitnessLog(0, cmd, 0, witness_size_));
#endif
    }
    return false;
  }
}

int Witness::remove(const shared_ptr<Marshallable>& cmd) {
  if (cmd->kind_ != MarshallDeputy::CMD_TPC_BATCH) {
    SimpleRWCommand parsed_cmd = SimpleRWCommand(cmd);
    bool removed = candidates_[parsed_cmd.key_].remove(SimpleRWCommand::CombineInt32(parsed_cmd.cmd_id_.first, parsed_cmd.cmd_id_.second));
    if (removed) {
      witness_size_distribution_.mid_time_append(--witness_size_);
      // if (candidates_[parsed_cmd.key_].size() == 0)
      //   candidates_.erase(parsed_cmd.key_);
    }
#ifdef WITNESS_LOG_DEBUG
    witness_log_.push_back(WitnessLog(1, cmd, removed, witness_size_));
#endif
    return removed;
  } else {
    auto cmds = dynamic_pointer_cast<TpcBatchCommand>(cmd);
    int total_removed = 0;
    for (auto& c: cmds->cmds_) {
      SimpleRWCommand parsed_cmd = SimpleRWCommand(c);
      bool removed = candidates_[parsed_cmd.key_].remove(SimpleRWCommand::CombineInt32(parsed_cmd.cmd_id_.first, parsed_cmd.cmd_id_.second));
      if (removed) {
        witness_size_distribution_.mid_time_append(--witness_size_);
        total_removed++;
        // if (candidates_[parsed_cmd.key_].size() == 0)
        //   candidates_.erase(parsed_cmd.key_);
      }
#ifdef WITNESS_LOG_DEBUG
      witness_log_.push_back(WitnessLog(1, c, removed, witness_size_));
#endif
    }
    return total_removed;
  }
}

void Witness::set_belongs_to_leader(bool belongs_to_leader) {
  belongs_to_leader_ = belongs_to_leader;
}

std::vector<double> Witness::witness_size_distribution() {
  // Log_info("witness 50pct %d %.2f" , witness_size_distribution_.count(), witness_size_distribution_.pct50());
  // Log_info("witness 90pct %d %.2f" , witness_size_distribution_.count(), witness_size_distribution_.pct90());
  // Log_info("witness 99pct %d %.2f" , witness_size_distribution_.count(), witness_size_distribution_.pct99());
  // Log_info("witness ave %d %.2f" , witness_size_distribution_.count(), witness_size_distribution_.ave());
  std::vector<double> ret;
  ret.push_back(witness_size_distribution_.pct50());
  ret.push_back(witness_size_distribution_.pct90());
  ret.push_back(witness_size_distribution_.pct99());
  ret.push_back(witness_size_distribution_.ave());
  // Log_info("witness ret %.2f %.2f %.2f %.2f", ret[0], ret[1], ret[2], ret[3]);
  return ret;
}

#ifdef WITNESS_LOG_DEBUG
void Witness::print_log() {
  if (witness_log_.size() == 0)
    return;
  for (int i = 0; i < witness_log_.size(); i++) {
    witness_log_[i].print(witness_log_[0].time_);
  }
}
#endif


} // namespace janus
