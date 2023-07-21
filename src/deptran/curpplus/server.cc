

#include "server.h"
// #include "paxos_worker.h"

namespace janus {

// bool_t CurpPlusServer::check_fast_path_validation(key_t key) {
//   std::lock_guard<std::recursive_mutex> lock(mtx_);
//   // [CURP] TODO: generalize
//   // Log_info("[CURP] Key = %d", key);
//   if (curp_log_cols_[key] == nullptr)
//     curp_log_cols_[key] = make_shared<CurpPlusDataCol>();
//   if (0 == curp_log_cols_[key]->count_)
//     return true;
//   shared_ptr<CurpPlusData> final_slot = curp_log_cols_[key]->Tail();
//   return !(final_slot->is_finish_ && final_slot->finish_countdown_);
// }

// value_t CurpPlusServer::read(key_t key) {
//   if (curp_log_cols_[key] == nullptr)
//     curp_log_cols_[key] = make_shared<CurpPlusDataCol>();
//   if (0 == curp_log_cols_[key]->count_)
//       return 0;
//   shared_ptr<Marshallable> cmd = curp_log_cols_[key]->Tail()->committed_cmd_;
//   shared_ptr<SimpleRWCommand> parsed_cmd_ = make_shared<SimpleRWCommand>(cmd);
//   value_t value = dynamic_pointer_cast<SimpleRWCommand>(parsed_cmd_)->value_;
//   return value;
// }

// slotid_t CurpPlusServer::append_cmd(key_t key, const shared_ptr<Marshallable>& cmd) {
//   // [CURP] TODO: maybe use a variable to lock
//   std::lock_guard<std::recursive_mutex> lock(mtx_);
//   if (curp_log_cols_[key] == nullptr)
//     curp_log_cols_[key] = make_shared<CurpPlusDataCol>();
//   shared_ptr<CurpPlusDataCol> col = curp_log_cols_[key];
//   size_t idx = ++curp_log_cols_[key]->count_;
//   verify(col->logs_[idx] == nullptr);
//   col->logs_[idx] = make_shared<CurpPlusData>();
//   col->logs_[idx]->fast_accepted_cmd_ = cmd;
//   // Log_info("[CURP] Loc %d Site %d append log on[%d][%d]", loc_id_, site_id_, key, idx);
//   return idx;
// }

void CurpPlusServer::Setup() {
  verify(commo_ != nullptr);
  Log_info("Setup this=%p, this->loc_id_=%d, this->commo_==%p",  (void*)this, this->loc_id_, (void*)this->commo_);
}

// void CurpPlusServer::OnDispatch(const int32_t& client_id,
//                                   const int32_t& cmd_id_in_client,
//                                   const shared_ptr<Marshallable>& cmd,
//                                   bool_t* accepted,
//                                   MarshallDeputy* pos_deputy,
//                                   value_t* result,
//                                   siteid_t* coo_id,
//                                   const function<void()> &cb) {
//   std::lock_guard<std::recursive_mutex> lock(mtx_);
//   shared_ptr<SimpleRWCommand> parsed_cmd_ = make_shared<SimpleRWCommand>(cmd);
//   key_t key = dynamic_pointer_cast<SimpleRWCommand>(parsed_cmd_)->key_;
//   bool_t fast_path_validation = check_fast_path_validation(key);
//   std::shared_ptr<Position> pos = make_shared<Position>(MarshallDeputy::POSITION_CLASSIC, 2);
//   if (!fast_path_validation) {
//     *accepted = false;
//     pos->set(0, -1);
//     pos->set(1, -1);
//     result = 0;
//   } else {
//     *accepted = true;
//     if (parsed_cmd_->type_ == SimpleRWCommand::CmdType::Read) {
//       pos->set(0, -1);
//       pos->set(1, -1);
//       *result = read(key);
//     } else if (parsed_cmd_->type_ == SimpleRWCommand::CmdType::Write) {
//       slotid_t new_slot_pos = append_cmd(key, cmd);
//       pos->set(0, key);
//       pos->set(1, new_slot_pos);
//       *result = 1;
//     } else {
//       verify(0);
//     }
//   }
//   int k = pos->get(0);
//   int v = pos->get(1);
//   Log_info("k=%d v=%d", k, v);
//   pos_deputy =  new MarshallDeputy(pos);
//   shared_ptr<IntEvent> sq_quorum = commo()->ForwardResultToCoordinator(partition_id_, cmd, *pos.get(), *accepted);
//   cb();
// }

// void CurpPlusServer::OnPoorDispatch(const int32_t& client_id,
//                                   const int32_t& cmd_id_in_client,
//                                   const shared_ptr<Marshallable>& cmd,
//                                   bool_t* accepted,
//                                   pos_t* pos0,
//                                   pos_t* pos1,
//                                   value_t* result,
//                                   siteid_t* coo_id,
//                                   const function<void()> &cb) {
//   std::lock_guard<std::recursive_mutex> lock(mtx_);
//   shared_ptr<SimpleRWCommand> parsed_cmd_ = make_shared<SimpleRWCommand>(cmd);
//   key_t key = dynamic_pointer_cast<SimpleRWCommand>(parsed_cmd_)->key_;
//   bool_t fast_path_validation = check_fast_path_validation(key);
//   std::shared_ptr<Position> pos = make_shared<Position>(MarshallDeputy::POSITION_CLASSIC, 2);
//   if (!fast_path_validation) {
//     Log_info("[CURP] OnPoorDispatch Branch 1");
//     *accepted = false;
//     *pos0 = -1;
//     *pos1 = -1;
//     *result = 0;
//   } else {
//     *accepted = true;
//     if (parsed_cmd_->type_ == SimpleRWCommand::CmdType::Read) {
//       Log_info("[CURP] OnPoorDispatch Branch 2");
//       *pos0 = -1;
//       *pos1 = -1;
//       *result = read(key);
//     } else if (parsed_cmd_->type_ == SimpleRWCommand::CmdType::Write) {
//       Log_info("[CURP] OnPoorDispatch Branch 3");
//       slotid_t new_slot_pos = append_cmd(key, cmd);
//       *pos0 = key;
//       *pos1 = new_slot_pos;
//       *result = 1;
//     } else {
//       verify(0);
//     }
//   }
//   *coo_id = 0;
//   int k = *pos0;
//   int v = *pos1;
//   pos->set(0, *pos0);
//   pos->set(1, *pos1);
//   // Log_info("[CURP] OnPoorDispatch k=%d v=%d", k, v);
//   shared_ptr<IntEvent> sq_quorum = commo()->ForwardResultToCoordinator(partition_id_, cmd, *pos.get(), *accepted);
//   Log_info("[CURP] OnPoorDispatch Before cb()");
//   cb();
// }

// void CurpPlusServer::OnWaitCommit(const int32_t& client_id,
//                                     const int32_t& cmd_id_in_client,
//                                     bool_t* committed,
//                                     const function<void()> &cb) {
//   std::lock_guard<std::recursive_mutex> lock(mtx_);
  
//   cb();
// }

// void CurpPlusServer::OnForward(const shared_ptr<Position>& pos,
//                                 const shared_ptr<Marshallable>& cmd,
//                                 const bool_t& accepted) {
//   std::lock_guard<std::recursive_mutex> lock(mtx_);
//   if (!accepted) return;
//   pair<int, int> accepted_and_max_accepted = curp_response_storage_[make_pair<pos_t, pos_t>(pos->get(0), pos->get(1))].append_response(cmd);
//   int accepted_num = accepted_and_max_accepted.first;
//   int max_accepted_num = accepted_and_max_accepted.second;
//   int par_id_ = 0; // TODO: change to real par_id_;
//   int n_replica = Config::GetConfig()->GetPartitionSize(par_id_);
//   // [CURP] TODO: change back to function
//   // if (accepted_num >= commo()->CurpFastQuorumSize(n_replica)) {
//   if (accepted_num >= (n_replica * 3 - 1) / 4 + 1) {
//     Commit(pos, cmd);
// #ifdef CURP_TIME_DEBUG
//     struct timeval tp;
//     gettimeofday(&tp, NULL);
//     Log_info("[CURP] [3-] [tx=%d] Before app_next_ %.3f", dynamic_pointer_cast<TpcCommitCommand>(cmd)->tx_id_, tp.tv_sec * 1000 + tp.tv_usec / 1000.0);
// #endif
//   // } else if (accepted_num >= commo()->quorumSize(n_replica) && max_accepted_num >= commo()->smallQuorumSize(n_replica)) {
//   } else if ((accepted_num >= n_replica - ((n_replica + 1) / 2 - 1)) && (max_accepted_num >= (n_replica - 1) / 4 + 1)) {
//     // [CURP] TODO: check this condition
//     shared_ptr<CurpPlusCoordinatorAcceptQuorumEvent> quorum = commo()->BroadcastCoordinatorAccept(partition_id_, pos, cmd);
//     quorum->Wait();
//     if (quorum->Yes()) {
//       Commit(pos, cmd);
//     } else if (quorum->No()) {
//       verify(0);
//     } else {
//       verify(0);
//     }
//   } else {
//     // do nothing
//   }
// }

// void CurpPlusServer::OnCoordinatorAccept(const shared_ptr<Position>& pos,
//                                           const shared_ptr<Marshallable>& cmd,
//                                           bool_t* accepted,
//                                           const function<void()> &cb) {
//   std::lock_guard<std::recursive_mutex> lock(mtx_);
//   shared_ptr<SimpleRWCommand> parsed_cmd_ = make_shared<SimpleRWCommand>(cmd);
//   key_t key = dynamic_pointer_cast<SimpleRWCommand>(parsed_cmd_)->key_;
//   pos_t pos0 = pos->get(0);
//   pos_t pos1 = pos->get(1);
//   // Log_info("[CURP] Loc %d Site %d OnCoordinatorAccept access log[%d][%d]", loc_id_, site_id_, pos0, pos1);
//   shared_ptr<CurpPlusData> log = curp_log_cols_[pos->get(0)]->logs_[pos->get(1)];
//   if (log->status_ != CurpPlusData::CurpPlusStatus::committed) {
//     log->last_accepted_ = cmd;
//     log->last_accepted_ballot_ = 0;
//     log->last_accepted_status_ = CurpPlusData::CurpPlusStatus::accepted;
//     *accepted = true;
//   } else {
//     *accepted = false;
//   }
//   cb();
// }

// void CurpPlusServer::OnPrepare(const shared_ptr<Position>& pos,
//                                 const ballot_t& ballot,
//                                 bool_t* accepted,
//                                 ballot_t* seen_ballot,
//                                 int* last_accepted_status,
//                                 shared_ptr<Marshallable>* last_accepted_cmd,
//                                 ballot_t* last_accepted_ballot,
//                                 const function<void()> &cb) {
//   std::lock_guard<std::recursive_mutex> lock(mtx_);
//   shared_ptr<CurpPlusData> log = curp_log_cols_[pos->get(0)]->logs_[pos->get(1)];
//   if (ballot > log->max_ballot_seen_) {
//     log->max_ballot_seen_ = ballot;
//     log->status_ = CurpPlusData::CurpPlusStatus::prepared;
//     *accepted = true;
//   } else {
//     *accepted = false;
//   }
//   *seen_ballot = log->max_ballot_seen_;
//   *last_accepted_status = log->last_accepted_status_;
//   *last_accepted_cmd = log->last_accepted_;
//   *last_accepted_ballot =  log->last_accepted_ballot_;
//   cb();
// }

// void CurpPlusServer::OnAccept(const shared_ptr<Position>& pos,
//                               const shared_ptr<Marshallable>& cmd,
//                               const ballot_t& ballot,
//                               bool_t* accepted,
//                               ballot_t* seen_ballot,
//                               const function<void()> &cb) {
//   std::lock_guard<std::recursive_mutex> lock(mtx_);
//   shared_ptr<CurpPlusData> log = curp_log_cols_[pos->get(0)]->logs_[pos->get(1)];
//   if (ballot >= log->max_ballot_seen_) {
//     log->accepted_cmd_ = cmd;
//     log->max_ballot_seen_ = ballot;
//     log->max_ballot_accepted_ = ballot;
//     log->status_ = CurpPlusData::CurpPlusStatus::accepted;
//     log->last_accepted_ = cmd;
//     log->last_accepted_ballot_ = ballot;
//     log->last_accepted_status_ = CurpPlusData::CurpPlusStatus::accepted;
//     *accepted = true;
//   } else {
//     *accepted = false;
//   }
//   *seen_ballot = log->max_ballot_seen_;
//   cb();
// }

// void CurpPlusServer::OnCommit(const shared_ptr<Position>& pos,
//                               const shared_ptr<Marshallable>& cmd) {
//   std::lock_guard<std::recursive_mutex> lock(mtx_);
//   key_t key = pos->get(0);
//   slotid_t slot_id = pos->get(1);
//   shared_ptr<CurpPlusDataCol> col = curp_log_cols_[key];
//   shared_ptr<CurpPlusData> instance = col->logs_[slot_id];
//   instance->status_ = CurpPlusData::CurpPlusStatus::committed;
//   instance->committed_cmd_ = cmd;
//   instance->last_accepted_ = cmd;
//   instance->last_accepted_status_ = CurpPlusData::CurpPlusStatus::committed;

//   if (slot_id > col->max_committed_slot_) {
//     col->max_committed_slot_ = slot_id;
//   }
//   verify(slot_id > col->max_executed_slot_);
//   if (in_applying_logs_) {
//     return;
//   }
//   in_applying_logs_ = true;
//   for (slotid_t id = col->max_executed_slot_ + 1; id <= col->max_committed_slot_; id++) {
//     shared_ptr<CurpPlusData> next_instance = col->logs_[id];
//     if (next_instance) {
//       // for now, FINISH cmd also have a global id
//       next_instance->global_id_ = ApplyForNewGlobalID();
//       if (!next_instance->is_finish_) {
//         app_next_(*next_instance->committed_cmd_);
//         Log_debug("curp par:%d loc:%d executed slot %lx now", partition_id_, loc_id_, id);
//         n_commit_++;
//       }
//       col->max_executed_slot_++;
//     } else {
//       break;
//     }
//   }

//   // TODO should support snapshot for freeing memory.
//   // for now just free anything 1000 slots before.
//   // TODO recover this
//   // int i = min_active_slot_;
//   // while (i + 1000 < max_executed_slot_) {
//   //   logs_.erase(i);
//   //   i++;
//   // }
//   // min_active_slot_ = i;
//   in_applying_logs_ = false;
// }

// void CurpPlusServer::Commit(shared_ptr<Position> pos,
//                             shared_ptr<Marshallable> cmd) {
//   OnCommit(pos, cmd);
//   commo()->BroadcastCommit(partition_id_, pos, cmd, site_id_);
// }

// slotid_t CurpPlusServer::ApplyForNewGlobalID() {
//   std::lock_guard<std::recursive_mutex> lock(mtx_);
//   return curp_global_id_hinter_++;
// }

// slotid_t CurpPlusServer::OriginalProtocolApplyForNewGlobalID(key_t key) {
//   shared_ptr<CurpPlusDataCol> col = curp_log_cols_[key];
//   // [CURP] TODO: What to do here ?????
//   if (col->count_ == 0 || !col->Tail()->is_finish_ || 0 == col->Tail()->finish_countdown_)
//     return 0;
//   if (col->max_committed_slot_ == col->count_)
//     return ApplyForNewGlobalID();
//   else
//     return 0;
// }

// UniqueCmdID CurpPlusServer::GetUniqueCmdID(shared_ptr<Marshallable> cmd) {
//   verify(cmd->kind_ == MarshallDeputy::CONTAINER_CMD);
//   shared_ptr<CmdData> casted_cmd = dynamic_pointer_cast<CmdData>(cmd);
//   UniqueCmdID cmd_id;
//   cmd_id.client_id_ = casted_cmd->client_id_;
//   cmd_id.cmd_id_ = casted_cmd->cmd_id_in_client_;
//   return cmd_id;
// }

} // namespace janus
