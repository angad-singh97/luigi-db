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

TxLogServer::TxLogServer() : mtx_() {
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
                                  siteid_t* coo_id,
                                  const function<void()> &cb) {
  // // used for debug
  // *accepted = false;
  // *ver = 0;
  // *result = 0;
  // *coo_id = 0;
  
  std::lock_guard<std::recursive_mutex> lock(mtx_);
  n_fast_path_attempted_++;
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
  if ((next_instance->status_ == CurpPlusData::CurpPlusStatus::INIT || SimpleRWCommand::GetCmdID(next_instance->GetCmd()) == make_pair(client_id, cmd_id_in_client))
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
  } else {
#ifdef CURP_FULL_LOG_DEBUG
    Log_info("[CURP] loc=%d OnCurpDispatch Reject cmd<%d, %d>%s since position(%d, %d) has status %d finish_countdown_[%d]=%d", loc_id_, client_id, cmd_id_in_client, parsed_cmd_->cmd_to_string().c_str(), key, curp_log_cols_[key]->NextVersion(), next_instance->status_, key, finish_countdown_[key]);
#endif
    if (client_id == -1) {
      int a = 1 + 1;
    }
    *accepted = false;
    *ver = -1;
    *result = -1;
  }
  *coo_id = 0;
  verify(curp_log_cols_[key]->logs_[curp_log_cols_[key]->NextVersion()] != nullptr);
#ifdef CURP_FULL_LOG_DEBUG
  Log_info("[CURP] About to CurpForwardResultToCoordinator, accepted=%d key=%d ver=%d result=%d", *accepted, key, *ver, *result);
#endif
  shared_ptr<IntEvent> sq_quorum = commo()->CurpForwardResultToCoordinator(partition_id_, *accepted, *ver, cmd);
  cb();
}

void TxLogServer::OnCurpWaitCommit(const int32_t& client_id,
                                    const int32_t& cmd_id_in_client,
                                    bool_t* committed,
                                    value_t* commit_result,
                                    const function<void()> &cb) {
  std::lock_guard<std::recursive_mutex> lock(mtx_);
  pair<int32_t, int32_t> cmd_id = make_pair(client_id, cmd_id_in_client);
  if (executed_results_[cmd_id] == nullptr)
    executed_results_[cmd_id] = make_shared<CommitNotification>();
  executed_results_[cmd_id]->committed_ = committed;
  executed_results_[cmd_id]->commit_result_ = commit_result;
  executed_results_[cmd_id]->commit_callback_ = cb;
  executed_results_[cmd_id]->client_stored_ = true;
  executed_results_[cmd_id]->receive_time_ = SimpleRWCommand::GetCurrentMsTime();
  commit_timeout_list_.push_back(executed_results_[cmd_id]);
#ifdef CURP_CONFLICT_DEBUG
  Log_info("[CURP] Client Stored commit callback for cmd<%d, %d>", client_id, cmd_id_in_client);
#endif
  if (executed_results_[cmd_id]->coordinator_stored_ && !executed_results_[cmd_id]->coordinator_replied_) {
   *committed = executed_results_[cmd_id]->coordinator_commit_result_;
   executed_results_[cmd_id]->coordinator_replied_ = true;
#ifdef CURP_CONFLICT_DEBUG
    Log_info("[CURP] Client Triggered commit callback for cmd<%d, %d>", client_id, cmd_id_in_client);
#endif
    cb();
  }
  Reactor::CreateSpEvent<TimeoutEvent>(CURP_WAIT_COMMIT_TIMEOUT * 1000)->Wait();
  if (!executed_results_[cmd_id]->coordinator_replied_) {
#ifdef CURP_FULL_LOG_DEBUG
    Log_info("[CURP] cmd<%d, %d> WaitCommitTimeout, about to original protocol", cmd_id.first, cmd_id.second);
#endif
    *executed_results_[cmd_id]->committed_ = false;
    *executed_results_[cmd_id]->commit_result_ = 0;
    executed_results_[cmd_id]->commit_callback_();
    executed_results_[cmd_id]->coordinator_replied_ = true;
    // original_protocol_submit_count_++;
  }
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
  if (curp_response_storage_[make_pair(key, ver)] == nullptr) {
      curp_response_storage_[make_pair(key, ver)] = make_shared<ResponseData>();
      response_pack = curp_response_storage_[make_pair(key, ver)];
      response_pack->first_seen_time_ = SimpleRWCommand::GetCurrentMsTime();
      response_pack->pos_of_this_pack = make_pair(key, ver);
  } else {
    response_pack = curp_response_storage_[make_pair(key, ver)];
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
  Log_info("[CURP] Forward Judgement of cmd<%d, %d>: %d %d, time_elapses=%.3f", 
    SimpleRWCommand::GetCmdID(cmd).first, 
    SimpleRWCommand::GetCmdID(cmd).second, 
    max_accepted_num >= CurpFastQuorumSize(n_replica), 
    (time_elapses > CURP_FAST_PATH_TIMEOUT || max_accepted_num + (n_replica - response_pack->received_count_) < CurpFastQuorumSize(n_replica))
      && accepted_num >= CurpQuorumSize(n_replica),
    time_elapses
  );
#endif
  if (max_accepted_num >= CurpFastQuorumSize(n_replica)) {
    // Branch 1
    if (response_pack->done_) return;
    response_pack->done_ = true;
    shared_ptr<Marshallable> max_cmd = response_pack->GetMaxCmd();
    // [CURP] TODO: verify(cmd == max_cmd)
    CurpCommit(ver, max_cmd);
    curp_fast_path_success_count_++;
  } else if ( (time_elapses > CURP_FAST_PATH_TIMEOUT || max_accepted_num + (n_replica - response_pack->received_count_) < CurpFastQuorumSize(n_replica))
      && accepted_num >= CurpQuorumSize(n_replica) ) {
    // branch 2
    if (response_pack->done_) return;
    response_pack->done_ = true;
    shared_ptr<Marshallable> max_cmd = response_pack->GetMaxCmd();
    CurpAccept(ver, 1, max_cmd);
  } 
  // else if (time_elapses > CURP_FAST_PATH_TIMEOUT && response_pack->received_count_ >= CurpQuorumSize(n_replica)) {
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
}

void TxLogServer::CurpPrepare(key_t key,
                              ver_t ver,
                              ballot_t ballot) {
#ifdef CURP_FULL_LOG_DEBUG
  Log_info("[CURP] loc=%d CurpPrepare(key=%d, ver=%d, ballot=%d)", loc_id_, key, ver, ballot);
#endif
  shared_ptr<CurpPrepareQuorumEvent> e = commo()->CurpBroadcastPrepare(partition_id_, key, ver, ballot);
  e->Wait();
  shared_ptr<CurpPlusData> log = GetOrCreateCurpLog(key, ver);
  log->max_seen_ballot_ = max(log->max_seen_ballot_, e->GetMaxSeenBallot());
  if (e->CommitYes()) {
#ifdef CURP_FULL_LOG_DEBUG
    Log_info("[CURP] loc=%d CurpPrepare(key=%d, ver=%d, ballot=%d) Branch 1: CommitYes", loc_id_, key, ver, ballot);
#endif
    CurpCommit(ver, e->GetCommittedCmd());
  } else if (e->FastAcceptYes()) {
#ifdef CURP_FULL_LOG_DEBUG
    Log_info("[CURP] loc=%d CurpPrepare(key=%d, ver=%d, ballot=%d) Branch 2: FastAcceptYes", loc_id_, key, ver, ballot);
#endif
    CurpAccept(ver, ballot, e->GetFastAcceptedCmd());
  } else if (e->AcceptYes()) {
#ifdef CURP_FULL_LOG_DEBUG
    Log_info("[CURP] loc=%d CurpPrepare(key=%d, ver=%d, ballot=%d) Branch 3: AcceptYes", loc_id_, key, ver, ballot);
#endif
    CurpAccept(ver, ballot, log->GetCmd());
  } else if (e->Yes()) {
#ifdef CURP_FULL_LOG_DEBUG
    Log_info("[CURP] loc=%d CurpPrepare(key=%d, ver=%d, ballot=%d) Branch 4: Yes", loc_id_, key, ver, ballot);
#endif
    // [CURP] TODO: optimize: Accept self value(cmd) if not nullptr
    CurpAccept(ver, ballot, MakeNoOpCmd(partition_id_));
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
    CurpCommit(ver, cmd);
  }
}

void TxLogServer::CurpCommit(ver_t ver,
                            shared_ptr<Marshallable> cmd) {
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
#ifdef CURP_CONFLICT_DEBUG
  
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
  if (ballot > log->max_seen_ballot_) {
#ifdef CURP_FULL_LOG_DEBUG
    Log_info("[CURP] loc=%d OnCurpPrepare(key=%d, ver=%d, ballot=%d) > local_ballot=%d, success, ret status=%d", loc_id_, key, ver, ballot, log->max_seen_ballot_, log->status_);
#endif
    log->max_seen_ballot_ = ballot;
    // log->status_ = CurpPlusData::CurpPlusStatus::PREPARED;
    *accepted = true;
  } else {
#ifdef CURP_FULL_LOG_DEBUG
    Log_info("[CURP] loc=%d OnCurpPrepare(key=%d, ver=%d, ballot=%d) <= local_ballot=%d, fail, ret status=%d", loc_id_, key, ver, ballot, log->max_seen_ballot_, log->status_);
#endif
    *accepted = false;
  }
  *status = log->status_;
  *last_accepted_ballot = log->last_accepted_ballot_;
  md_cmd->SetMarshallable(log->GetCmd());
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
  if (ballot >= log->max_seen_ballot_) {
#ifdef CURP_FULL_LOG_DEBUG
    Log_info("[CURP] loc=%d OnCurpAccept(key=%d, ver=%d, ballot=%d, cmd=[%s]) >= local_ballot=%d, success", loc_id_, parsed_cmd_->key_, ver, ballot, parsed_cmd_->cmd_to_string().c_str(), log->max_seen_ballot_);
#endif
    log->UpdateCmd(cmd);
    log->max_seen_ballot_ = ballot;
    log->last_accepted_ballot_ = ballot;
    log->status_ = CurpPlusData::CurpPlusStatus::ACCEPTED;
    *accepted = true;
  } else {
#ifdef CURP_FULL_LOG_DEBUG
    Log_info("[CURP] loc=%d OnCurpAccept(key=%d, ver=%d, ballot=%d, cmd=[%s]) < local_ballot=%d, fail", loc_id_, parsed_cmd_->key_, ver, ballot, parsed_cmd_->cmd_to_string().c_str(), log->max_seen_ballot_);
#endif
    *accepted = false;
  }
  *seen_ballot = log->max_seen_ballot_;
  cb();
}

void TxLogServer::OnCurpCommit(const ver_t& ver,
                              const shared_ptr<Marshallable>& cmd) {
#ifdef CURP_FULL_LOG_DEBUG
  Log_info("[CURP] loc=%d OnCurpCommit ver=%d cmd<%d, %d>[%s]", loc_id_, ver, SimpleRWCommand::GetCmdID(cmd).first, SimpleRWCommand::GetCmdID(cmd).second, SimpleRWCommand(cmd).cmd_to_string().c_str());
#endif  
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

  if (curp_log_cols_[key]->NextVersion() == ver) {
    while (instance->status_ == CurpPlusData::CurpPlusStatus::COMMITTED) {
      curp_log_cols_[key]->Execute(ver);
      instance = curp_log_cols_[key]->NextInstance();
    }
  }

}

// [CURP] TODO: discard this
void TxLogServer::OnOriginalSubmit(shared_ptr<Marshallable> &cmd,
                                    const rrr::i64& dep_id,
                                    bool_t* slow,
                                    const function<void()> &cb) {
  // Log_info("enter OnOriginalSubmit");
  original_protocol_submit_count_++;
  auto sp_tx = dynamic_pointer_cast<TxClassic>(GetTx(dynamic_pointer_cast<TpcCommitCommand>(cmd)->tx_id_));
  shared_ptr<Coordinator> coo{CreateRepCoord(dep_id)};
  coo->svr_workers_g = svr_workers_g;
  coo->Submit(cmd);
  // [CURP] TODO: deal with slow
  // sp_tx->commit_result->Wait();
  // *slow = coo->slow_;
  *slow = false;
  cb();
}

shared_ptr<Marshallable> MakeFinishCmd(parid_t par_id, int cmd_id, key_t key, value_t value) {
  map<int32_t, Value> m;
  m[0] = key;
  m[1] = value;
  shared_ptr<TxPieceData> txPieceData = make_shared<TxPieceData>();
  txPieceData->client_id_ = -1;
  txPieceData->cmd_id_in_client_ = cmd_id;
  txPieceData->partition_id_ = par_id;
  txPieceData->input.insert(m);
  txPieceData->type_ = RW_BENCHMARK_FINISH;
  shared_ptr<vector<shared_ptr<TxPieceData>>> sp_vec_piece = make_shared<vector<shared_ptr<TxPieceData>>>();
  sp_vec_piece->push_back(txPieceData);
  shared_ptr<VecPieceData> vecPiece = make_shared<VecPieceData>();
  vecPiece->sp_vec_piece_data_ = sp_vec_piece;
  return vecPiece;
}

shared_ptr<Marshallable> MakeNoOpCmd(parid_t par_id) {
  map<int32_t, Value> m;
  m[0] = 0;
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
    svr_->instance_commit_timeout_pool_.AddTimeoutInstance(shared_from_this());
    svr_->curp_log_cols_[key_]->CheckHoles(ver_);
  }
}

void CurpPlusDataCol::CheckHoles(ver_t ver) {
  for (int i = latest_executed_ver_ + 1; i < ver; i++) {
    svr_->instance_commit_timeout_pool_.AddTimeoutInstance(GetOrCreate(i));
  }
}

shared_ptr<Marshallable> CurpPlusData::GetCmd() {
  return cmd_;
}

void TxLogServer::CurpSkipFastpath(int32_t cmd_id, shared_ptr<Marshallable> &cmd) {
  original_protocol_submit_count_++;
  key_t key = SimpleRWCommand::GetKey(cmd);
  while (finish_countdown_[key] == 0) {
#ifdef CURP_FULL_LOG_DEBUG
    Log_info("[CURP] Attempted to commit FIN for cmd<%d> at svr %d", cmd_id, loc_id_);
#endif
    auto e = commo()->CurpBroadcastDispatch(MakeFinishCmd(partition_id_, cmd_id, key, 100));
    e->Wait();
  }
#ifdef CURP_FULL_LOG_DEBUG
  Log_info("[CURP] Success skip fastpath for cmd<%d> at svr %d", cmd_id, loc_id_);
#endif
  finish_countdown_[key]--;
}

CurpPlusData::CurpPlusData(TxLogServer* svr, key_t key, ver_t ver, CurpPlusStatus status, const shared_ptr<Marshallable> &cmd, ballot_t ballot)
  : svr_(svr), key_(key), ver_(ver), status_(status), max_seen_ballot_(ballot) {
  verify(!svr_->assigned_[make_pair(key, ver)]);
  svr_->assigned_[make_pair(key, ver)] = true;
#ifdef CURP_FULL_LOG_DEBUG
  Log_info("[CURP] Create instance for pos(%d, %d)", key, ver);
#endif
  key_ = key;
  ver_ = ver;
  UpdateCmd(cmd);
}

shared_ptr<CurpPlusData> CurpPlusDataCol::GetOrCreate(ver_t ver) {
  if (logs_[ver] == nullptr) {
    verify(!svr_->assigned_[make_pair(key_, ver)]);
    logs_[ver] = make_shared<CurpPlusData>(svr_, key_, ver, CurpPlusData::CurpPlusStatus::INIT, nullptr, 0);
    svr_->assigned_[make_pair(key_, ver)] = true;
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
    pair<int32_t, int32_t> cmd_id = SimpleRWCommand::GetCmdID(instance->GetCmd());

    // store execute result
    if (svr_->executed_results_[cmd_id] == nullptr)
      svr_->executed_results_[cmd_id] = make_shared<CommitNotification>();
    svr_->executed_results_[cmd_id]->coordinator_commit_result_ = result;
    svr_->executed_results_[cmd_id]->coordinator_stored_ = true;
#ifdef CURP_FULL_LOG_DEBUG
    Log_info("[CURP] Server Stored commit result for cmd<%d, %d>", cmd_id.first, cmd_id.second);
#endif

    // reply execute result if asked
    if (svr_->executed_results_[cmd_id]->client_stored_ && !svr_->executed_results_[cmd_id]->coordinator_replied_) {
      *svr_->executed_results_[cmd_id]->commit_result_ = svr_->executed_results_[cmd_id]->coordinator_commit_result_;
      svr_->executed_results_[cmd_id]->coordinator_replied_ = true;
      svr_->executed_results_[cmd_id]->commit_callback_();
#ifdef CURP_FULL_LOG_DEBUG
      Log_info("[CURP] Server Triggered commit callback for cmd<%d, %d>", cmd_id.first, cmd_id.second);
#endif
    } else { 
#ifdef CURP_FULL_LOG_DEBUG
      Log_info("[CURP] Server Fail to Trigger commit callback for cmd<%d, %d> for judgement stored=%d replied=%d", cmd_id.first, cmd_id.second, svr_->executed_results_[cmd_id]->client_stored_, svr_->executed_results_[cmd_id]->coordinator_replied_);
#endif
    }
  }
    
  instance->status_ = CurpPlusData::CurpPlusStatus::EXECUTED;

  latest_executed_ver_ = ver;
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

void InstanceCommitTimeoutPool::TimeoutLoop() {
  while (true) {
    Reactor::CreateSpEvent<TimeoutEvent>(CURP_INSTANCE_COMMIT_TIMEOUT * 1000 + rand() % (CURP_INSTANCE_COMMIT_TIMEOUT * 1000 * 3))->Wait();
    for (set<pair<double, shared_ptr<CurpPlusData>>>::iterator it = pool_.begin(); it != pool_.end(); ++it) {
      if (it->first < SimpleRWCommand::GetCurrentMsTime()) {
        if (it->second->status_ == CurpPlusData::CurpPlusStatus::COMMITTED || it->second->status_ == CurpPlusData::CurpPlusStatus::EXECUTED) {
          in_pool_.erase(it->second->GetPos());
        } else {
          it->second->PrepareInstance();
          AddTimeoutInstance(it->second, true);
          pool_.erase(it);
        }
      }
    }
  }
}
void InstanceCommitTimeoutPool::AddTimeoutInstance(shared_ptr<CurpPlusData> instance, bool repeat_insert) {
  if (instance->status_ == CurpPlusData::CurpPlusStatus::COMMITTED || instance->status_ == CurpPlusData::CurpPlusStatus::EXECUTED)
    return;
  double trigger_ms_timeout = CURP_INSTANCE_COMMIT_TIMEOUT + rand() % (CURP_INSTANCE_COMMIT_TIMEOUT * 3 * 1000) / 1000.0;
  if (repeat_insert) {
    pool_.insert(make_pair(SimpleRWCommand::GetCurrentMsTime() + trigger_ms_timeout, instance));
    return;
  }
  pair<key_t, ver_t> pos = instance->GetPos();
  if (in_pool_.count(pos) == 0) {
    in_pool_.insert(pos);
    pool_.insert(make_pair(SimpleRWCommand::GetCurrentMsTime() + trigger_ms_timeout, instance));
  }
}

void CurpPlusData::PrepareInstance() {
  max_seen_ballot_++;
  svr_->CurpPrepare(key_, ver_, max_seen_ballot_);
}

} // namespace janus
