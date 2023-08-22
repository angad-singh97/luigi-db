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
#include "RW_command.h"

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
    // Log_info("[copilot+] CreateTx");
    ret = CreateTx(tid, ro);
  } else {
    //Log_info("found");
    // Log_info("[copilot+] GetTx");
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
  // verify(cli2svr_dispatch_count > 0 && cli2svr_commit_count > 0);
  if (cli2svr_dispatch.count() || cli2svr_commit.count())
    Log_info("[CURP] loc_id_=%d site_id_=%d curp_executed_committed_max_gap_=%d \
    curp_fast_path_success_count_=%d curp_coordinator_accept_count_=%d original_protocol_submit_count_=%d total=%d \
    cli2svr_dispatch 50% = %.2f ms cli2svr_dispatch 90% = %.2f ms cli2svr_dispatch 99% = %.2f ms \
    cli2svr_commit 50% = %.2f ms cli2svr_commit 90% = %.2f ms cli2svr_commit_max 99% = %.2f ms",
              loc_id_, site_id_, curp_executed_committed_max_gap_, 
              curp_fast_path_success_count_, curp_coordinator_accept_count_, original_protocol_submit_count_,
              curp_fast_path_success_count_ + curp_coordinator_accept_count_ + original_protocol_submit_count_,
              cli2svr_dispatch.pct50(), cli2svr_dispatch.pct90(), cli2svr_dispatch.pct99(),
              cli2svr_commit.pct50(), cli2svr_commit.pct90(), cli2svr_commit.pct99());
  else
    Log_info("[CURP] loc_id_=%d site_id_=%d No Count / Latency Measured", loc_id_, site_id_);
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

key_t TxLogServer::get_key_from_marshallable(shared_ptr<Marshallable> cmd) {
  SimpleRWCommand simple_cmd(cmd);
  return simple_cmd.key_;
}

bool_t TxLogServer::check_fast_path_validation(key_t key) {
  std::lock_guard<std::recursive_mutex> lock(mtx_);
  // [CURP] TODO: generalize
  // Log_info("[CURP] Key = %d", key);
  if (curp_log_cols_[key] == nullptr)
    curp_log_cols_[key] = make_shared<CurpPlusDataCol>();
  if (0 == curp_log_cols_[key]->count_)
    return true;
  shared_ptr<CurpPlusData> final_slot = curp_log_cols_[key]->Tail();
  return !(final_slot->is_finish_ && final_slot->finish_countdown_);
}

value_t TxLogServer::read(key_t key) {
  if (curp_log_cols_[key] == nullptr)
    curp_log_cols_[key] = make_shared<CurpPlusDataCol>();
  if (0 == curp_log_cols_[key]->count_)
      return 0;
  shared_ptr<Marshallable> cmd = curp_log_cols_[key]->Tail()->committed_cmd_;
  shared_ptr<SimpleRWCommand> parsed_cmd_ = make_shared<SimpleRWCommand>(cmd);
  value_t value = dynamic_pointer_cast<SimpleRWCommand>(parsed_cmd_)->value_;
  return value;
}

slotid_t TxLogServer::append_cmd(key_t key, const shared_ptr<Marshallable>& cmd) {
  // [CURP] TODO: maybe use a variable to lock
  std::lock_guard<std::recursive_mutex> lock(mtx_);
  // Log_info("[CURP] server %d append_cmd on key %d", loc_id_, key);
  // verify(curp_log_cols_[key]->count_ == 0);
  // if (curp_log_cols_[key] == nullptr)
  //   curp_log_cols_[key] = make_shared<CurpPlusDataCol>();
  shared_ptr<CurpPlusDataCol> col = curp_log_cols_[key];
  size_t idx = ++curp_log_cols_[key]->count_;
  verify(!col->logs_.count(idx));
  col->logs_[idx] = make_shared<CurpPlusData>();
  col->logs_[idx]->fast_accepted_cmd_ = cmd;
  // Log_info("[CURP] Loc %d Site %d append log on[%d][%d]", loc_id_, site_id_, key, idx);
  return idx;
}

void TxLogServer::OnCurpPoorDispatch(const int32_t& client_id,
                                  const int32_t& cmd_id_in_client,
                                  const shared_ptr<Marshallable>& cmd,
                                  bool_t* accepted,
                                  pos_t* pos0,
                                  pos_t* pos1,
                                  value_t* result,
                                  siteid_t* coo_id,
                                  const function<void()> &cb) {
  std::lock_guard<std::recursive_mutex> lock(mtx_);
  n_fast_path_attempted_++;
  shared_ptr<SimpleRWCommand> parsed_cmd_ = make_shared<SimpleRWCommand>(cmd);
  key_t key = dynamic_pointer_cast<SimpleRWCommand>(parsed_cmd_)->key_;
  // Log_info("[CURP] OnCurpPoorDispatch loc_id_ %d cmd (%d, %d) key=%d", loc_id_, client_id, cmd_id_in_client, key);
  bool_t fast_path_validation = check_fast_path_validation(key);
  std::shared_ptr<Position> pos = make_shared<Position>(MarshallDeputy::POSITION_CLASSIC, 2);

  struct timeval tp;
  gettimeofday(&tp, NULL);
  // double sent_time = ((VecPieceData*)(dynamic_pointer_cast<TpcCommitCommand>(cmd)->cmd_.get()))->time_sent_from_client_;
  double sent_time = (dynamic_pointer_cast<VecPieceData>(cmd))->time_sent_from_client_;
  double current_time = tp.tv_sec * 1000 + tp.tv_usec / 1000.0;
  cli2svr_dispatch.append(current_time - sent_time);

  if (!fast_path_validation) {
    // Log_info("[CURP] OnPoorDispatch Branch 1");
    *accepted = false;
    *pos0 = -1;
    *pos1 = -1;
    *result = 0;
  } else {
    *accepted = true;
    if (parsed_cmd_->type_ == SimpleRWCommand::CmdType::Read) {
      // Log_info("[CURP] OnPoorDispatch Branch 2");
      *pos0 = -1;
      *pos1 = -1;
      *result = read(key);
    } else if (parsed_cmd_->type_ == SimpleRWCommand::CmdType::Write) {
      // Log_info("[CURP] OnPoorDispatch Branch 3");
      slotid_t new_slot_pos = append_cmd(key, cmd);
      *pos0 = key;
      *pos1 = new_slot_pos;
      *result = 1;
    } else {
      verify(0);
    }
  }
  *coo_id = 0;
  int k = *pos0;
  int v = *pos1;
  pos->set(0, *pos0);
  pos->set(1, *pos1);
  // Log_info("[CURP] OnPoorDispatch k=%d v=%d", k, v);
  shared_ptr<IntEvent> sq_quorum = commo()->CurpForwardResultToCoordinator(partition_id_, cmd, *pos.get(), *accepted);
  // Log_info("[CURP] OnPoorDispatch Before cb()");
  cb();
}

void TxLogServer::OnCurpWaitCommit(const int32_t& client_id,
                                    const int32_t& cmd_id_in_client,
                                    bool_t* committed,
                                    const function<void()> &cb) {
  std::lock_guard<std::recursive_mutex> lock(mtx_);
  pair<int32_t, int32_t> cmd_id = make_pair(client_id, cmd_id_in_client);
  if (commit_results_[cmd_id] == nullptr)
    commit_results_[cmd_id] = make_shared<CommitNotification>();
  commit_results_[cmd_id]->committed_ = committed;
  commit_results_[cmd_id]->commit_callback_ = cb;
  commit_results_[cmd_id]->client_stored_ = true;
  commit_results_[cmd_id]->receive_time_ = SimpleRWCommand::GetCurrentMsTime();
  commit_timeout_list_.push_back(commit_results_[cmd_id]);
#ifdef CURP_CONFLICT_DEBUG
  Log_info("[CURP] Client Stored commit callback for cmd<%d, %d>", client_id, cmd_id_in_client);
#endif
  if (commit_results_[cmd_id]->coordinator_stored_ && !commit_results_[cmd_id]->coordinator_replied_) {
   *committed = commit_results_[cmd_id]->coordinator_commit_result_;
   commit_results_[cmd_id]->coordinator_replied_ = true;
#ifdef CURP_CONFLICT_DEBUG
    Log_info("[CURP] Client Triggered commit callback for cmd<%d, %d>", client_id, cmd_id_in_client);
#endif
    cb();
  }
  Reactor::CreateSpEvent<NeverEvent>()->Wait(CURP_WAIT_COMMIT_TIMEOUT * 1000);
  if (!commit_results_[cmd_id]->coordinator_replied_) {
    *commit_results_[cmd_id]->committed_ = false;
    commit_results_[cmd_id]->commit_callback_();
    commit_results_[cmd_id]->coordinator_replied_ = true;
    original_protocol_submit_count_++;
  }
}

void TxLogServer::OnCurpForward(const shared_ptr<Position>& pos,
                                const shared_ptr<Marshallable>& cmd,
                                const bool_t& accepted) {
  std::lock_guard<std::recursive_mutex> lock(mtx_);
  shared_ptr<ResponseData> response_pack = nullptr;
  if (curp_response_storage_[make_pair<pos_t, pos_t>(pos->get(0), pos->get(1))] == nullptr) {
      curp_response_storage_[make_pair<pos_t, pos_t>(pos->get(0), pos->get(1))] = make_shared<ResponseData>();
      response_pack = curp_response_storage_[make_pair<pos_t, pos_t>(pos->get(0), pos->get(1))];
      response_pack->first_seen_time_ = SimpleRWCommand::GetCurrentMsTime();
      response_pack->pos_of_this_pack = make_pair(pos->get(0), pos->get(1));
  } else {
    response_pack = curp_response_storage_[make_pair<pos_t, pos_t>(pos->get(0), pos->get(1))];
  }
  response_pack->received_count_++;
#ifdef CURP_CONFLICT_DEBUG
  Log_info("[CURP] Received Curp Forward of cmd<%d, %d> at pos(%d, %d)", SimpleRWCommand::GetCmdID(cmd).first, SimpleRWCommand::GetCmdID(cmd).second, pos->get(0), pos->get(1));
#endif
  if (!accepted) {
    return;
  }
  pair<int, int> accepted_and_max_accepted = response_pack->append_response(cmd);
  int accepted_num = accepted_and_max_accepted.first;
  int max_accepted_num = accepted_and_max_accepted.second;
  int par_id_ = 0; // TODO: change to real par_id_;
  int n_replica = Config::GetConfig()->GetPartitionSize(par_id_);

  // Log_info("[CURP] !!! %p site=%d done=%d accepted_num=%d max_accepted_num=%d", (void*)this, site_id_, response_pack->done_, accepted_num, max_accepted_num);

  double current_time = SimpleRWCommand::GetCurrentMsTime();
  double time_elapses = current_time - response_pack->first_seen_time_;

#ifdef CURP_CONFLICT_DEBUG
  Log_info("[CURP] Forward Judgement of cmd<%d, %d>: %d %d %d %d", 
    SimpleRWCommand::GetCmdID(cmd).first, 
    SimpleRWCommand::GetCmdID(cmd).second, 
    time_elapses <= CURP_FAST_PATH_TIMEOUT && max_accepted_num >= CurpFastQuorumSize(n_replica), 
    ((time_elapses <= CURP_FAST_PATH_TIMEOUT && max_accepted_num + (n_replica - response_pack->received_count_) < CurpFastQuorumSize(n_replica)) || (time_elapses > CURP_FAST_PATH_TIMEOUT)),
    (accepted_num >= CurpQuorumSize(n_replica) && max_accepted_num >= CurpSmallQuorumSize(n_replica)),
    (accepted_num + (n_replica - response_pack->received_count_) < CurpQuorumSize(n_replica) || max_accepted_num + (n_replica - response_pack->received_count_) < CurpSmallQuorumSize(n_replica)));
#endif
  if (time_elapses <= CURP_FAST_PATH_TIMEOUT && max_accepted_num >= CurpFastQuorumSize(n_replica)) {
    if (response_pack->done_) return;
    response_pack->done_ = true;
    CurpCommit(pos, cmd);
    curp_fast_path_success_count_++;
  } else if ( ((time_elapses <= CURP_FAST_PATH_TIMEOUT && max_accepted_num + (n_replica - response_pack->received_count_) < CurpFastQuorumSize(n_replica)) || (time_elapses > CURP_FAST_PATH_TIMEOUT))
      && (accepted_num >= CurpQuorumSize(n_replica) && max_accepted_num >= CurpSmallQuorumSize(n_replica)) ) {
      // [CURP] TODO: check this condition
      shared_ptr<Marshallable> max_cmd = response_pack->GetMaxCmd();
#ifdef CURP_CONFLICT_DEBUG
      Log_info("[CURP] Broadcast Coordinator Accept for cmd<%d, %d> at pos(%d, %d)", SimpleRWCommand::GetCmdID(max_cmd).first, SimpleRWCommand::GetCmdID(max_cmd).second, pos->get(0), pos->get(1));
#endif
      if (response_pack->done_) return;
      response_pack->done_ = true;
      shared_ptr<CurpPlusCoordinatorAcceptQuorumEvent> quorum = commo()->CurpBroadcastCoordinatorAccept(partition_id_, pos, max_cmd);
      quorum->Wait();
#ifdef CURP_CONFLICT_DEBUG
      Log_info("[CURP] Coordinator Accept for cmd<%d, %d> Has Result", SimpleRWCommand::GetCmdID(cmd).first, SimpleRWCommand::GetCmdID(cmd).second);
#endif
      if (quorum->Yes()) {
        CurpCommit(pos, max_cmd);
        curp_coordinator_accept_count_++;
      } else if (quorum->No()) {
        verify(0);
        // [CURP] TODO: Do original protocol
      } else {
        verify(0);
      }
  } else if (accepted_num + (n_replica - response_pack->received_count_) < CurpQuorumSize(n_replica) || max_accepted_num + (n_replica - response_pack->received_count_) < CurpSmallQuorumSize(n_replica)) {
    // abort and do original protocol
    shared_ptr<Marshallable> no_op = make_shared<TpcNoopCommand>();
    if (response_pack->done_) return;
    response_pack->done_ = true;
    CurpCommit(pos, no_op);
  } else {
    // do nothing
  }
}

void TxLogServer::OnCurpCoordinatorAccept(const shared_ptr<Position>& pos,
                                          const shared_ptr<Marshallable>& cmd,
                                          bool_t* accepted,
                                          const function<void()> &cb) {
  n_fast_path_failed_++;
  std::lock_guard<std::recursive_mutex> lock(mtx_);
  shared_ptr<SimpleRWCommand> parsed_cmd_ = make_shared<SimpleRWCommand>(cmd);
  key_t key = dynamic_pointer_cast<SimpleRWCommand>(parsed_cmd_)->key_;
  pos_t pos0 = pos->get(0);
  pos_t pos1 = pos->get(1);
  // Log_info("[CURP] Loc %d Site %d OnCoordinatorAccept access log[%d][%d]", loc_id_, site_id_, pos0, pos1);
  shared_ptr<CurpPlusData> log = GetOrCreateCurpLog(pos->get(0), pos->get(1));
  if (log->status_ != CurpPlusData::CurpPlusStatus::committed) {
    log->last_accepted_ = cmd;
    log->last_accepted_ballot_ = 0;
    log->last_accepted_status_ = CurpPlusData::CurpPlusStatus::accepted;
    *accepted = true;
  } else {
    *accepted = false;
  }
#ifdef CURP_CONFLICT_DEBUG
  Log_info("[CURP] Server %d replied %d to CoordinatorAccept cmd<%d, %d> pos(%d, %d)", site_id_, *accepted, SimpleRWCommand::GetCmdID(cmd).first, SimpleRWCommand::GetCmdID(cmd).second, pos0, pos1);
#endif
  cb();
}

void TxLogServer::OnCurpPrepare(const shared_ptr<Position>& pos,
                                const ballot_t& ballot,
                                bool_t* accepted,
                                ballot_t* seen_ballot,
                                int* last_accepted_status,
                                shared_ptr<Marshallable>* last_accepted_cmd,
                                ballot_t* last_accepted_ballot,
                                const function<void()> &cb) {
  std::lock_guard<std::recursive_mutex> lock(mtx_);
  shared_ptr<CurpPlusData> log = GetOrCreateCurpLog(pos->get(0), pos->get(1));
  if (ballot > log->max_ballot_seen_) {
    log->max_ballot_seen_ = ballot;
    log->status_ = CurpPlusData::CurpPlusStatus::prepared;
    *accepted = true;
  } else {
    *accepted = false;
  }
  *seen_ballot = log->max_ballot_seen_;
  *last_accepted_status = log->last_accepted_status_;
  *last_accepted_cmd = log->last_accepted_;
  *last_accepted_ballot =  log->last_accepted_ballot_;
  cb();
}

void TxLogServer::OnCurpAccept(const shared_ptr<Position>& pos,
                              const shared_ptr<Marshallable>& cmd,
                              const ballot_t& ballot,
                              bool_t* accepted,
                              ballot_t* seen_ballot,
                              const function<void()> &cb) {
  std::lock_guard<std::recursive_mutex> lock(mtx_);
  shared_ptr<CurpPlusData> log = GetOrCreateCurpLog(pos->get(0), pos->get(1));
  if (ballot >= log->max_ballot_seen_) {
    log->accepted_cmd_ = cmd;
    log->max_ballot_seen_ = ballot;
    log->max_ballot_accepted_ = ballot;
    log->status_ = CurpPlusData::CurpPlusStatus::accepted;
    log->last_accepted_ = cmd;
    log->last_accepted_ballot_ = ballot;
    log->last_accepted_status_ = CurpPlusData::CurpPlusStatus::accepted;
    *accepted = true;
  } else {
    *accepted = false;
  }
  *seen_ballot = log->max_ballot_seen_;
  cb();
}

void TxLogServer::OnCurpCommit(const shared_ptr<Position>& pos,
                              const shared_ptr<Marshallable>& cmd) {
  std::lock_guard<std::recursive_mutex> lock(mtx_);
  pos_t key = pos->get(0);
  pos_t slot_id = pos->get(1);

  if (cmd->kind_ != MarshallDeputy::CMD_NOOP) {
    double sent_time = (dynamic_pointer_cast<VecPieceData>(cmd))->time_sent_from_client_;
    double current_time = SimpleRWCommand::GetCurrentMsTime();
    cli2svr_commit.append(current_time - sent_time);
  }
  
  if (cmd->kind_ != MarshallDeputy::CMD_NOOP) {
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
#ifdef CURP_CONFLICT_DEBUG
    Log_info("[CURP] Svr %d Commit cmd<%d, %d> pos (%d, %d)", loc_id_, vector0->client_id_, vector0->cmd_id_in_client_, key, slot_id);
#endif
  } else {
#ifdef CURP_CONFLICT_DEBUG
    Log_info("[CURP] Svr %d Commit No-Op pos (%d, %d)", loc_id_, key, slot_id);
#endif
  }
  
  shared_ptr<CurpPlusDataCol> col = curp_log_cols_[key];
  shared_ptr<CurpPlusData> instance = GetOrCreateCurpLog(key, slot_id);
  instance->status_ = CurpPlusData::CurpPlusStatus::committed;
  instance->committed_cmd_ = cmd;
  instance->last_accepted_ = cmd;
  instance->last_accepted_status_ = CurpPlusData::CurpPlusStatus::committed;

  if (slot_id > col->max_committed_slot_) {
    curp_executed_committed_gap_ += slot_id - col->max_committed_slot_;
    if (curp_executed_committed_gap_ > curp_executed_committed_max_gap_)
      curp_executed_committed_max_gap_ = curp_executed_committed_gap_;
    col->max_committed_slot_ = slot_id;
  }

  if (cmd->kind_ != MarshallDeputy::CMD_NOOP) {
    UniqueCmdID unique_cmd = GetUniqueCmdID(cmd);
  }

  // Log_info("[CURP] CurpCommit at loc %d site %d cmd(%d, %d)", loc_id_, site_id_, unique_cmd.client_id_, unique_cmd.cmd_id_);
  // Log_info("[CURP] slot_id=%d col->max_executed_slot_=%d", slot_id, col->max_executed_slot_);
  verify(slot_id > col->max_executed_slot_);
  if (curp_in_applying_logs_) {
    return;
  }
  curp_in_applying_logs_ = true;
  for (slotid_t id = col->max_executed_slot_ + 1; id <= col->max_committed_slot_; id++) {
    shared_ptr<CurpPlusData> next_instance = col->logs_[id];
    if (next_instance && next_instance->status_ == CurpPlusData::CurpPlusStatus::committed) {
      verify(next_instance->committed_cmd_);
      // for now, FINISH cmd also have a global id
      next_instance->global_id_ = ApplyForNewGlobalID();
      if (next_instance->committed_cmd_ && !next_instance->is_finish_) {
        // app_next_(*next_instance->committed_cmd_); // this is for old non-curp-broadcast
        if (next_instance->committed_cmd_->kind_ == MarshallDeputy::CMD_NOOP) {
          // do nothing
          executed_logs_[next_instance->global_id_] = make_pair<int32_t, int32_t>(-1, -1);
        } else {
          verify(next_instance->committed_cmd_->kind_ == MarshallDeputy::CMD_VEC_PIECE);
          pair<int32_t, int32_t> cmd_id = SimpleRWCommand::GetCmdID(next_instance->committed_cmd_);
          executed_logs_[next_instance->global_id_] = cmd_id;
          if (commit_results_[cmd_id] == nullptr)
            commit_results_[cmd_id] = make_shared<CommitNotification>();
          commit_results_[cmd_id]->coordinator_commit_result_ = true;
          commit_results_[cmd_id]->coordinator_stored_ = true;
#ifdef CURP_CONFLICT_DEBUG
          Log_info("[CURP] Server Stored commit result for cmd<%d, %d>", cmd_id.first, cmd_id.second);
#endif
          if (commit_results_[cmd_id]->client_stored_ && !commit_results_[cmd_id]->coordinator_replied_) {
            *commit_results_[cmd_id]->committed_ = commit_results_[cmd_id]->coordinator_commit_result_;
            commit_results_[cmd_id]->coordinator_replied_ = true;
#ifdef CURP_CONFLICT_DEBUG
            Log_info("[CURP] Server Triggered commit callback for cmd<%d, %d>", cmd_id.first, cmd_id.second);
#endif
            commit_results_[cmd_id]->commit_callback_();
          } else {
#ifdef CURP_CONFLICT_DEBUG
            Log_info("[CURP] Server Fail to Trigger commit callback for cmd<%d, %d> for judgement stored=%d replied=%d", cmd_id.first, cmd_id.second, commit_results_[cmd_id]->client_stored_, commit_results_[cmd_id]->coordinator_replied_);
#endif
          }
        }
        next_instance->status_ = CurpPlusData::CurpPlusStatus::executed;
        Log_debug("curp par:%d loc:%d executed slot %lx now", partition_id_, loc_id_, id);
        // n_commit_++;
      }
      curp_executed_committed_gap_--;
      col->max_executed_slot_++;
    } else {
      break;
    }
  }

  // for (int i = commit_timeout_solved_count_; i < commit_timeout_list_.size(); i++) {
  //   if (SimpleRWCommand::GetCurrentMsTime() - commit_timeout_list_[i]->receive_time_ > CURP_WAIT_COMMIT_TIMEOUT) {
  //     if (!commit_timeout_list_[i]->coordinator_replied_) {
  //       *commit_timeout_list_[i]->committed_ = false;
  //       commit_timeout_list_[i]->commit_callback_();
  //       commit_timeout_list_[i]->coordinator_replied_ = true;
  //       original_protocol_submit_count_++;
  //     }
  //     commit_timeout_solved_count_++;
  //   } else {
  //     break;
  //   }
  // }
  // TODO should support snapshot for freeing memory.
  // for now just free anything 1000 slots before.
  // TODO recover this
  // int i = min_active_slot_;
  // while (i + 1000 < max_executed_slot_) {
  //   logs_.erase(i);
  //   i++;
  // }
  // min_active_slot_ = i;
  curp_in_applying_logs_ = false;
}

void TxLogServer::CurpCommit(shared_ptr<Position> pos,
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
    Log_info("[CURP] [CurpCommit] svr %d Commit No-Op pos(%d, %d)", loc_id_, pos->get(0), pos->get(1));
  } else {
    shared_ptr<TxPieceData> vector0 = *(sp_vec_piece->begin());
    Log_info("[CURP] [CurpCommit] svr %d Commit cmd<%d, %d> pos(%d, %d)", loc_id_, vector0->client_id_, vector0->cmd_id_in_client_, pos->get(0), pos->get(1));
  }
#endif
  OnCurpCommit(pos, cmd);
  commo()->CurpBroadcastCommit(partition_id_, pos, cmd, loc_id_);
}

slotid_t TxLogServer::ApplyForNewGlobalID() {
  std::lock_guard<std::recursive_mutex> lock(mtx_);
  return curp_global_id_hinter_++;
}

slotid_t TxLogServer::OriginalProtocolApplyForNewGlobalID(key_t key) {
  shared_ptr<CurpPlusDataCol> col = curp_log_cols_[key];
  // [CURP] TODO: What to do here ?????
  if (col->count_ == 0 || !col->Tail()->is_finish_ || 0 == col->Tail()->finish_countdown_)
    return 0;
  if (col->max_committed_slot_ == col->count_)
    return ApplyForNewGlobalID();
  else
    return 0;
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

shared_ptr<CurpPlusData> TxLogServer::GetCurpLog(pos_t pos0, pos_t pos1) {
  // verify(pos1 == 0);
  if (curp_log_cols_.count(pos0)) {
    if (curp_log_cols_[pos0]->logs_.count(pos1)) {
      return curp_log_cols_[pos0]->logs_[pos1];
    } else {
      return nullptr;
    }
  } else {
    return nullptr;
  }
}

shared_ptr<CurpPlusData> TxLogServer::GetOrCreateCurpLog(pos_t pos0, pos_t pos1) {
  // verify(pos1 == 1);
  std::lock_guard<std::recursive_mutex> lock(mtx_);
  if (curp_log_cols_[pos0] == nullptr)
    curp_log_cols_[pos0] = make_shared<CurpPlusDataCol>();
  if (curp_log_cols_[pos0]->logs_[pos1] == nullptr)
    curp_log_cols_[pos0]->logs_[pos1] = make_shared<CurpPlusData>();
  if (pos1 > curp_log_cols_[pos0]->count_)
    curp_log_cols_[pos0]->count_ = pos1;
  return curp_log_cols_[pos0]->logs_[pos1];
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

void TxLogServer::PrintExecutedLogs() {
  string file_name = "loc_" + to_string(loc_id_) + "_site_" + to_string(site_id_) + ".log";
  freopen(file_name.c_str(), "w", stdout);
  for (auto it = executed_logs_.begin(); it != executed_logs_.end(); it++) {
    printf("slot: %d, cmd<%d, %d>\n", it->first, it->second.first, it->second.second);
  }
  freopen(NULL, "w", stdout);
}

} // namespace janus
