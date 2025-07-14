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
  }
#endif
  std::vector<double> witness_size_distribution = witness_.witness_size_distribution();
  Log_info("loc_id=%d witness size distribution 50pct %.2f 90pct %.2f 99pct %.2f ave %.2f",
    loc_id_, witness_size_distribution[0], witness_size_distribution[1], witness_size_distribution[2], witness_size_distribution[3]);
#ifdef WITNESS_LOG_DEBUG
  if (loc_id_ == 0 || loc_id_ == 1)
    witness_.print_log();
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
#ifdef JETPACK_DEDUPLICATE_OPTIMIZATION
  cmd_count_[cmd_id]++;
#endif
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

bool RevoveryCandidates::has_appeared(uint64_t cmd_id) {
  return cmd_count_[cmd_id];
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

bool Witness::has_appeared(const shared_ptr<Marshallable>& cmd) {
  // For a batched command, return whether all of them have appeared
  if (cmd->kind_ != MarshallDeputy::CMD_TPC_BATCH) {
    SimpleRWCommand parsed_cmd = SimpleRWCommand(cmd);
    uint64_t cmd_id = SimpleRWCommand::CombineInt32(parsed_cmd.cmd_id_.first, parsed_cmd.cmd_id_.second);
    return candidates_[parsed_cmd.key_].has_appeared(cmd_id);
  } else {
    auto cmds = dynamic_pointer_cast<TpcBatchCommand>(cmd);
    bool all_has_appeared = true;
    for (auto& c: cmds->cmds_) {
      SimpleRWCommand parsed_cmd = SimpleRWCommand(c);
      uint64_t cmd_id = SimpleRWCommand::CombineInt32(parsed_cmd.cmd_id_.first, parsed_cmd.cmd_id_.second);
      if (!candidates_[parsed_cmd.key_].has_appeared(cmd_id)) {
        all_has_appeared = false;
        break;
      }
    }
    return all_has_appeared;
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
