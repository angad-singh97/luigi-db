

#include "server.h"
// #include "paxos_worker.h"

namespace janus {

  bool_t CurpPlusServer::check_fast_path_validation(key_t key) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    // [CURP] TODO: generalize
    // Log_info("[CURP] Key = %d", key);
    if (log_cols_[key] == nullptr)
      log_cols_[key] = make_shared<CurpPlusDataCol>();
    // Log_info("[CURP] count_ = %d", log_cols_[key]->count_);
    // Log_info("[CURP] last_finish_pos_ = %d", log_cols_[key]->last_finish_pos_);
    return (log_cols_[key]->count_) - (log_cols_[key]->last_finish_pos_) > 1;
  }

  value_t CurpPlusServer::read(key_t key) {
    if (log_cols_[key] == nullptr)
      log_cols_[key] = make_shared<CurpPlusDataCol>();
    if (0 == log_cols_[key]->count_)
        return 0;
    shared_ptr<Marshallable> cmd = log_cols_[key]->logs_[log_cols_[key]->count_ - 1]->committed_cmd_;
    shared_ptr<SimpleRWCommand> parsed_cmd_ = make_shared<SimpleRWCommand>(cmd);
    value_t value = dynamic_pointer_cast<SimpleRWCommand>(parsed_cmd_)->value_;
    return value;
  }

  slotid_t CurpPlusServer::append_cmd(key_t key, const shared_ptr<Marshallable>& cmd) {
    // [CURP] TODO: whether lock?
    // std::lock_guard<std::recursive_mutex> lock(mtx_);
    if (log_cols_[key] == nullptr)
      log_cols_[key] = make_shared<CurpPlusDataCol>();
    shared_ptr<CurpPlusDataCol> col = log_cols_[key];
    size_t idx = log_cols_[key]->count_;
    verify(col->logs_[idx] == nullptr);
    col->logs_[idx] = make_shared<CurpPlusData>();
    col->logs_[idx]->fast_accepted_cmd_ = cmd;
    // log_cols_[key].logs_[log_cols_[key].count_]->fast_accepted_cmd_ = cmd;
    slotid_t append_pos = log_cols_[key]->count_;
    log_cols_[key]->count_++;
    // Log_info("[CURP] Loc %d Site %d append log on[%d][%d]", loc_id_, site_id_, key, idx);
    return append_pos;
  }

  void CurpPlusServer::Setup() {
    verify(commo_ != nullptr);
    Log_info("Setup this=%p, this->loc_id_=%d, this->commo_==%p",  (void*)this, this->loc_id_, (void*)this->commo_);
  }

  void CurpPlusServer::OnDispatch(const int32_t& client_id,
                                    const int32_t& cmd_id_in_client,
                                    const shared_ptr<Marshallable>& cmd,
                                    bool_t* accepted,
                                    MarshallDeputy* pos_deputy,
                                    value_t* result,
                                    siteid_t* coo_id,
                                    const function<void()> &cb) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    shared_ptr<SimpleRWCommand> parsed_cmd_ = make_shared<SimpleRWCommand>(cmd);
    key_t key = dynamic_pointer_cast<SimpleRWCommand>(parsed_cmd_)->key_;
    bool_t fast_path_validation = check_fast_path_validation(key);
    std::shared_ptr<Position> pos = make_shared<Position>(MarshallDeputy::POSITION_CLASSIC, 2);
    if (!fast_path_validation) {
      *accepted = false;
      pos->set(0, -1);
      pos->set(1, -1);
      result = 0;
    } else {
      *accepted = true;
      if (parsed_cmd_->type_ == SimpleRWCommand::CmdType::Read) {
        pos->set(0, -1);
        pos->set(1, -1);
        *result = read(key);
      } else if (parsed_cmd_->type_ == SimpleRWCommand::CmdType::Write) {
        slotid_t new_slot_pos = append_cmd(key, cmd);
        pos->set(0, key);
        pos->set(1, new_slot_pos);
        *result = 1;
      } else {
        verify(0);
      }
    }
    int k = pos->get(0);
    int v = pos->get(1);
    Log_info("k=%d v=%d", k, v);
    pos_deputy =  new MarshallDeputy(pos);
    shared_ptr<IntEvent> sq_quorum = commo()->ForwardResultToCoordinator(partition_id_, cmd, *pos.get(), *accepted);
    cb();
  }

  void CurpPlusServer::OnPoorDispatch(const int32_t& client_id,
                                    const int32_t& cmd_id_in_client,
                                    const shared_ptr<Marshallable>& cmd,
                                    bool_t* accepted,
                                    pos_t* pos0,
                                    pos_t* pos1,
                                    value_t* result,
                                    siteid_t* coo_id,
                                    const function<void()> &cb) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    shared_ptr<SimpleRWCommand> parsed_cmd_ = make_shared<SimpleRWCommand>(cmd);
    key_t key = dynamic_pointer_cast<SimpleRWCommand>(parsed_cmd_)->key_;
    bool_t fast_path_validation = check_fast_path_validation(key);
    std::shared_ptr<Position> pos = make_shared<Position>(MarshallDeputy::POSITION_CLASSIC, 2);
    if (!fast_path_validation) {
      *accepted = false;
      *pos0 = -1;
      *pos1 = -1;
      result = 0;
    } else {
      *accepted = true;
      if (parsed_cmd_->type_ == SimpleRWCommand::CmdType::Read) {
        *pos0 = -1;
        *pos1 = -1;
        *result = read(key);
      } else if (parsed_cmd_->type_ == SimpleRWCommand::CmdType::Write) {
        slotid_t new_slot_pos = append_cmd(key, cmd);
        *pos0 = key;
        *pos1 = new_slot_pos;
        *result = 1;
      } else {
        verify(0);
      }
    }
    int k = *pos0;
    int v = *pos1;
    pos->set(0, *pos0);
    pos->set(1, *pos1);
    // Log_info("[CURP] OnPoorDispatch k=%d v=%d", k, v);
    shared_ptr<IntEvent> sq_quorum = commo()->ForwardResultToCoordinator(partition_id_, cmd, *pos.get(), *accepted);
    cb();
  }
  
  void CurpPlusServer::OnWaitCommit(const int32_t& client_id,
                                      const int32_t& cmd_id_in_client,
                                      bool_t* committed,
                                      const function<void()> &cb) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    
    cb();
  }

  void CurpPlusServer::OnForward(const shared_ptr<Position>& pos,
                                  const shared_ptr<Marshallable>& cmd,
                                  const bool_t& accepted) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    if (!accepted) return;
    pair<int, int> accepted_and_max_accepted = response_storage_[make_pair<pos_t, pos_t>(pos->get(0), pos->get(1))].append_response(cmd);
    int accepted_num = accepted_and_max_accepted.first;
    int max_accepted_num = accepted_and_max_accepted.second;
    int par_id_ = 0; // TODO: change to real par_id_;
    int n_replica = Config::GetConfig()->GetPartitionSize(par_id_);
    shared_ptr<TpcCommitCommand> tpc_cmd = dynamic_pointer_cast<TpcCommitCommand>(cmd);
    // [CURP] TODO: ugly here, maybe problem
    tpc_cmd->ret_ = SUCCESS;
    if (accepted_num >= commo()->fastQuorumSize(n_replica)) {
      commo()->BroadcastCommit(partition_id_, pos, cmd);
      app_next_(*tpc_cmd);
    } else if (accepted_num >= commo()->quorumSize(n_replica) /*&& max_accepted_num >= commo()->smallQuorumSize(n_replica)*/) {
      // [CURP] TODO: check this condition
      shared_ptr<CurpPlusCoordinatorAcceptQuorumEvent> quorum = commo()->BroadcastCoordinatorAccept(partition_id_, pos, cmd);
      quorum->Wait();
      if (quorum->Yes()) {
        commo()->BroadcastCommit(partition_id_, pos, cmd);
        app_next_(*tpc_cmd);
      } else if (quorum->No()) {
        verify(0);
      } else {
        verify(0);
      }
    } else {
      // do nothing
    }
  }

  void CurpPlusServer::OnCoordinatorAccept(const shared_ptr<Position>& pos,
                                            const shared_ptr<Marshallable>& cmd,
                                            bool_t* accepted,
                                            const function<void()> &cb) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    shared_ptr<SimpleRWCommand> parsed_cmd_ = make_shared<SimpleRWCommand>(cmd);
    key_t key = dynamic_pointer_cast<SimpleRWCommand>(parsed_cmd_)->key_;
    pos_t pos0 = pos->get(0);
    pos_t pos1 = pos->get(1);
    // Log_info("[CURP] Loc %d Site %d OnCoordinatorAccept access log[%d][%d]", loc_id_, site_id_, pos0, pos1);
    shared_ptr<CurpPlusData> log = log_cols_[pos->get(0)]->logs_[pos->get(1)];
    if (log->status_ != CurpPlusData::CurpPlusStatus::committed) {
      log->last_accepted_ = cmd;
      log->last_accepted_ballot_ = 0;
      log->last_accepted_status_ = CurpPlusData::CurpPlusStatus::accepted;
      *accepted = true;
    } else {
      *accepted = false;
    }
    cb();
  }

  void CurpPlusServer::OnPrepare(const shared_ptr<Position>& pos,
                                  const ballot_t& ballot,
                                  bool_t* accepted,
                                  ballot_t* seen_ballot,
                                  int* last_accepted_status,
                                  shared_ptr<Marshallable>* last_accepted_cmd,
                                  ballot_t* last_accepted_ballot,
                                  const function<void()> &cb) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    shared_ptr<CurpPlusData> log = log_cols_[pos->get(0)]->logs_[pos->get(1)];
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

  void CurpPlusServer::OnAccept(const shared_ptr<Position>& pos,
                                const shared_ptr<Marshallable>& cmd,
                                const ballot_t& ballot,
                                bool_t* accepted,
                                ballot_t* seen_ballot,
                                const function<void()> &cb) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    shared_ptr<CurpPlusData> log = log_cols_[pos->get(0)]->logs_[pos->get(1)];
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

  void CurpPlusServer::OnCommit(const shared_ptr<Position>& pos,
                                const shared_ptr<Marshallable>& cmd) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    shared_ptr<CurpPlusData> log = log_cols_[pos->get(0)]->logs_[pos->get(1)];
    log->status_ = CurpPlusData::CurpPlusStatus::committed;
    log->committed_cmd_ = cmd;
    log->last_accepted_ = cmd;
    log->last_accepted_status_ = CurpPlusData::CurpPlusStatus::committed;
  }

} // namespace janus
