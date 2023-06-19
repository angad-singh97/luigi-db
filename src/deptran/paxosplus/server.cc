

#include "server.h"
// #include "paxos_worker.h"
#include "exec.h"

namespace janus {

/***************************************PLUS Begin***********************************************************/
  bool_t PaxosPlusServer::check_fast_path_validation(key_t key) {

  }

  value_t PaxosPlusServer::read(key_t key) {
    if (0 == log_cols_[key].count)
        return 0;
    // todo: get value from this cmd
    return log_cols_[key].logs_[log_cols_[key].count - 1]->committed_cmd_;
  }

  slotid_t PaxosPlusServer::append_cmd(key_t key, shared_ptr<Marshallable>& cmd) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    log_cols_[key].logs_[log_cols_[key].count]->fast_accepted_cmd_ = cmd;
    slotid_t append_pos = log_cols_[key].count;
    log_cols_[key].count++;
    return append_pos;
  }
/***************************************PLUS End***********************************************************/

// void PaxosServer::OnForward(shared_ptr<Marshallable> &cmd,
//                             uint64_t dep_id,
//                             uint64_t* coro_id,
//                             const function<void()> &cb){
//   Log_info("This paxos server is: %d", frame_->site_info_->id);
//   std::lock_guard<std::recursive_mutex> lock(mtx_);

//   auto config = Config::GetConfig();
//   rep_frame_ = Frame::GetFrame(config->replica_proto_);
//   rep_frame_->site_info_ = frame_->site_info_;
  
//   int n_io_threads = 1;
//   auto svr_poll_mgr_ = new rrr::PollMgr(n_io_threads);
//   auto rep_commo_ = rep_frame_->CreateCommo(svr_poll_mgr_);
//   if(rep_commo_){
//     rep_commo_->loc_id_ = frame_->site_info_->locale_id;
//   }
//   //rep_sched_->loc_id_ = site_info_->locale_id;;
//   //rep_sched_->partition_id_ = site_info_->partition_id_;

//   CreateRepCoord(dep_id)->Submit(cmd);
//   *coro_id = Coroutine::CurrentCoroutine()->id;
//   cb();
// }

// void PaxosServer::OnPrepare(slotid_t slot_id,
//                             ballot_t ballot,
//                             ballot_t *max_ballot,
//                             uint64_t* coro_id,
//                             const function<void()> &cb) {

//   std::lock_guard<std::recursive_mutex> lock(mtx_);
//   Log_debug("multi-paxos scheduler receives prepare for slot_id: %llx",
//             slot_id);
//   auto instance = GetInstance(slot_id);
//   verify(ballot != instance->max_ballot_seen_);
//   if (instance->max_ballot_seen_ < ballot) {
//     instance->max_ballot_seen_ = ballot;
//   } else {
//     // TODO if accepted anything, return;
//     verify(0);
//   }
//   *coro_id = Coroutine::CurrentCoroutine()->id;
//   *max_ballot = instance->max_ballot_seen_;
//   n_prepare_++;
//   cb();
// }


// void PaxosServer::OnAccept(const slotid_t slot_id,
// 		           const uint64_t time,
//                            const ballot_t ballot,
//                            shared_ptr<Marshallable> &cmd,
//                            ballot_t *max_ballot,
//                            uint64_t* coro_id,
//                            const function<void()> &cb) {
//   std::lock_guard<std::recursive_mutex> lock(mtx_);
//   Log_debug("multi-paxos scheduler accept for slot_id: %llx", slot_id);

//   auto instance = GetInstance(slot_id);
  
//   //TODO: might need to optimize this. we can vote yes on duplicates at least for now
//   //verify(instance->max_ballot_accepted_ < ballot);
  
//   if (instance->max_ballot_seen_ <= ballot) {
//     instance->max_ballot_seen_ = ballot;
//     instance->max_ballot_accepted_ = ballot;
//   } else {
//     // TODO
//     verify(0);
//   }

//   *coro_id = Coroutine::CurrentCoroutine()->id;
//   *max_ballot = instance->max_ballot_seen_;
//   n_accept_++;
//   cb();
// }

// void PaxosServer::OnCommit(const slotid_t slot_id,
//                            const ballot_t ballot,
//                            shared_ptr<Marshallable> &cmd) {
//   std::lock_guard<std::recursive_mutex> lock(mtx_);
//   Log_debug("multi-paxos scheduler decide for slot: %lx", slot_id);
//   auto instance = GetInstance(slot_id);
//   instance->committed_cmd_ = cmd;
//   if (slot_id > max_committed_slot_) {
//     max_committed_slot_ = slot_id;
//   }
//   verify(slot_id > max_executed_slot_);
//   // This prevents the log entry from being applied twice
//   if (in_applying_logs_) {
//     return;
//   }
//   in_applying_logs_ = true;
//   for (slotid_t id = max_executed_slot_ + 1; id <= max_committed_slot_; id++) {
//     auto next_instance = GetInstance(id);
//     if (next_instance->committed_cmd_) {
//       app_next_(*next_instance->committed_cmd_);
//       Log_debug("multi-paxos par:%d loc:%d executed slot %lx now", partition_id_, loc_id_, id);
//       max_executed_slot_++;
//       n_commit_++;
//     } else {
//       break;
//     }
//   }

//   // TODO should support snapshot for freeing memory.
//   // for now just free anything 1000 slots before.
//   int i = min_active_slot_;
//   while (i + 1000 < max_executed_slot_) {
//     logs_.erase(i);
//     i++;
//   }
//   min_active_slot_ = i;
//   in_applying_logs_ = false;
// }

// void PaxosServer::Setup() {
//   Log_info("Setup this=%p, this->loc_id_=%d, this->commo_==%p", 
//         (void*)this, this->loc_id_, (void*)this->commo_);
// }

  void PaxosPlusServer::OnForward(const shared_ptr<Position>& pos,
                                  const shared_ptr<Marshallable>& cmd,
                                  const bool_t& accepted,
                                  const function<void()> &cb) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    pair<int, int> accepted_and_max_accepted = response_storage_[make_pair<key_t, slotid_t>(pos->get_key(0), pos->get_slot(1))].append_response(cmd);
    int accepted = accepted_and_max_accepted.first;
    int max_accepted = accepted_and_max_accepted.second;
    int par_id_ = 0; // TODO: change to real par_id_;
    int n_replica = Config::GetConfig()->GetPartitionSize(par_id_);
    if (accepted >= commo()->fastQuorumSize(n_replica)) {
      commo()->BroadcastCommit(pos, cmd);
    } else if (accepted >= commo()->quorumSize(n_replica) && max_accepted >= commo()->smallQuorumSize(n_replica)) {
      commo()->BroadcastCoordinatorAccept(pos, cmd);
    } else {
      // do nothing
    }
    cb();
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
                                  shared_ptr<Marshallable> last_accepted_cmd,
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
                                const shared_ptr<Marshallable>& cmd,
                                const function<void()> &cb) {
    shared_ptr<PaxosPlusData> log = log_cols_[pos->get_key(0)].logs_[pos->get_slot(1)];
    log->status_ = PaxosPlusData::PaxosPlusStatus::committed;
    log->committed_cmd_ = cmd;
    log->last_accepted_ = cmd;
    log->last_accepted_status_ = PaxosPlusData::PaxosPlusStatus::committed;
    cb();
  }

} // namespace janus
