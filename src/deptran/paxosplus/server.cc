

#include "server.h"
// #include "paxos_worker.h"

namespace janus {

/***************************************PLUS Begin***********************************************************/
  bool_t PaxosPlusServer::check_fast_path_validation(key_t key) {
    // TODO
    return true;
  }

  value_t PaxosPlusServer::read(key_t key) {
    if (0 == log_cols_[key].count_)
        return 0;
    shared_ptr<Marshallable> cmd = log_cols_[key].logs_[log_cols_[key].count_ - 1]->committed_cmd_;
    shared_ptr<SimpleRWCommand> parsed_cmd_ = make_shared<SimpleRWCommand>(cmd);
    value_t value = dynamic_pointer_cast<SimpleRWCommand>(parsed_cmd_)->value_;
    return value;
  }

  slotid_t PaxosPlusServer::append_cmd(key_t key, shared_ptr<Marshallable>& cmd) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    log_cols_[key].logs_[log_cols_[key].count_]->fast_accepted_cmd_ = cmd;
    slotid_t append_pos = log_cols_[key].count_;
    log_cols_[key].count_++;
    return append_pos;
  }
/***************************************PLUS End***********************************************************/

  void PaxosPlusServer::OnForward(const shared_ptr<Position>& pos,
                                  const shared_ptr<Marshallable>& cmd,
                                  const bool_t& accepted) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    if (!accepted) return;
    pair<int, int> accepted_and_max_accepted = response_storage_[make_pair<key_t, slotid_t>(pos->get_key(0), pos->get_slot(1))].append_response(cmd);
    int accepted_num = accepted_and_max_accepted.first;
    int max_accepted_num = accepted_and_max_accepted.second;
    int par_id_ = 0; // TODO: change to real par_id_;
    int n_replica = Config::GetConfig()->GetPartitionSize(par_id_);
    if (accepted_num >= commo()->fastQuorumSize(n_replica)) {
      commo()->BroadcastCommit(partition_id_, pos, cmd);
    } else if (accepted_num >= commo()->quorumSize(n_replica) && max_accepted_num >= commo()->smallQuorumSize(n_replica)) {
      commo()->BroadcastCoordinatorAccept(partition_id_, pos, cmd);
    } else {
      // do nothing
    }
  }

  void PaxosPlusServer::OnCoordinatorAccept(const shared_ptr<Position>& pos,
                                            const shared_ptr<Marshallable>& cmd,
                                            bool_t* accepted,
                                            const function<void()> &cb) {
    shared_ptr<SimpleRWCommand> parsed_cmd_ = make_shared<SimpleRWCommand>(cmd);
    key_t key = dynamic_pointer_cast<SimpleRWCommand>(parsed_cmd_)->key_;
    shared_ptr<PaxosPlusData> log = log_cols_[pos->get_key(0)].logs_[pos->get_slot(1)];
    if (log->status_ != PaxosPlusData::PaxosPlusStatus::committed) {
      log->last_accepted_ = cmd;
      log->last_accepted_ballot_ = 0,
      log->last_accepted_status_ = PaxosPlusData::PaxosPlusStatus::accepted;
      *accepted = true;
    } else {
      *accepted = false;
    }
    cb();
  }

  void PaxosPlusServer::OnPrepare(const shared_ptr<Position>& pos,
                                  const ballot_t& ballot,
                                  bool_t* accepted,
                                  ballot_t* seen_ballot,
                                  int* last_accepted_status,
                                  shared_ptr<Marshallable>* last_accepted_cmd,
                                  ballot_t* last_accepted_ballot,
                                  const function<void()> &cb) {
    shared_ptr<PaxosPlusData> log = log_cols_[pos->get_key(0)].logs_[pos->get_slot(1)];
    if (ballot > log->max_ballot_seen_) {
      log->max_ballot_seen_ = ballot;
      log->status_ = PaxosPlusData::PaxosPlusStatus::prepared;
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

  void PaxosPlusServer::OnAccept(const shared_ptr<Position>& pos,
                                const shared_ptr<Marshallable>& cmd,
                                const ballot_t& ballot,
                                bool_t* accepted,
                                ballot_t* seen_ballot,
                                const function<void()> &cb) {
    shared_ptr<PaxosPlusData> log = log_cols_[pos->get_key(0)].logs_[pos->get_slot(1)];
    if (ballot >= log->max_ballot_seen_) {
      log->accepted_cmd_ = cmd;
      log->max_ballot_seen_ = ballot;
      log->max_ballot_accepted_ = ballot;
      log->status_ = PaxosPlusData::PaxosPlusStatus::accepted;
      log->last_accepted_ = cmd;
      log->last_accepted_ballot_ = ballot;
      log->last_accepted_status_ = PaxosPlusData::PaxosPlusStatus::accepted;
      *accepted = true;
    } else {
      *accepted = false;
    }
    *seen_ballot = log->max_ballot_seen_;
    cb();
  }

  void PaxosPlusServer::OnCommit(const shared_ptr<Position>& pos,
                                const shared_ptr<Marshallable>& cmd) {
    shared_ptr<PaxosPlusData> log = log_cols_[pos->get_key(0)].logs_[pos->get_slot(1)];
    log->status_ = PaxosPlusData::PaxosPlusStatus::committed;
    log->committed_cmd_ = cmd;
    log->last_accepted_ = cmd;
    log->last_accepted_status_ = PaxosPlusData::PaxosPlusStatus::committed;
  }

} // namespace janus
