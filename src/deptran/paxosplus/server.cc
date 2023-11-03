

#include "server.h"
// #include "paxos_worker.h"
#include "exec.h"

namespace janus {


void PaxosPlusServer::OnForward(shared_ptr<Marshallable> &cmd,
                            uint64_t dep_id,
                            uint64_t* coro_id,
                            const function<void()> &cb){
  Log_info("This paxos server is: %d", frame_->site_info_->id);
  std::lock_guard<std::recursive_mutex> lock(mtx_);

  auto config = Config::GetConfig();
  rep_frame_ = Frame::GetFrame(config->replica_proto_);
  rep_frame_->site_info_ = frame_->site_info_;
  
  int n_io_threads = 1;
  auto svr_poll_mgr_ = new rrr::PollMgr(n_io_threads);
  auto rep_commo_ = rep_frame_->CreateCommo(svr_poll_mgr_);
  if(rep_commo_){
    rep_commo_->loc_id_ = frame_->site_info_->locale_id;
  }
  //rep_sched_->loc_id_ = site_info_->locale_id;;
  //rep_sched_->partition_id_ = site_info_->partition_id_;

  CreateRepCoord(dep_id)->Submit(cmd);
  *coro_id = Coroutine::CurrentCoroutine()->id;
  WAN_WAIT
  cb();
}

void PaxosPlusServer::OnPrepare(slotid_t slot_id,
                            ballot_t ballot,
                            ballot_t *max_ballot,
                            uint64_t* coro_id,
                            const function<void()> &cb) {

  std::lock_guard<std::recursive_mutex> lock(mtx_);
  Log_debug("multi-paxos scheduler receives prepare for slot_id: %llx",
            slot_id);
  auto instance = GetInstance(slot_id);
  verify(ballot != instance->max_ballot_seen_);
  if (instance->max_ballot_seen_ < ballot) {
    instance->max_ballot_seen_ = ballot;
  } else {
    // TODO if accepted anything, return;
    verify(0);
  }
  *coro_id = Coroutine::CurrentCoroutine()->id;
  *max_ballot = instance->max_ballot_seen_;
  n_prepare_++;
  WAN_WAIT
  cb();
}


void PaxosPlusServer::OnAccept(const slotid_t slot_id,
		                       const uint64_t time,
                           const ballot_t ballot,
                           shared_ptr<Marshallable> &cmd,
                           ballot_t *max_ballot,
                           uint64_t* coro_id,
                           const function<void()> &cb) {
  std::lock_guard<std::recursive_mutex> lock(mtx_);
#ifdef CURP_FULL_LOG_DEBUG
  Log_info("[CURP] Paxos OnAccept cmd<%d, %d>", SimpleRWCommand::GetCmdID(cmd).first, SimpleRWCommand::GetCmdID(cmd).second);
#endif
#ifdef LATENCY_DEBUG
  cli2follower_recv_.append(SimpleRWCommand::GetCommandMsTimeElaps(cmd));
#endif
  Log_debug("multi-paxos scheduler accept for slot_id: %llx", slot_id);

  auto instance = GetInstance(slot_id);
  
  //TODO: might need to optimize this. we can vote yes on duplicates at least for now
  //verify(instance->max_ballot_accepted_ < ballot);
  
  if (instance->max_ballot_seen_ <= ballot) {
    instance->max_ballot_seen_ = ballot;
    instance->max_ballot_accepted_ = ballot;
  } else {
    // TODO
    verify(0);
  }

  *coro_id = Coroutine::CurrentCoroutine()->id;
  *max_ballot = instance->max_ballot_seen_;
  n_accept_++;
#ifdef LATENCY_DEBUG
  cli2follower_send_.append(SimpleRWCommand::GetCommandMsTimeElaps(cmd));
#endif
  WAN_WAIT
  cb();
}

void PaxosPlusServer::OnCommit(const slotid_t slot_id,
                           const ballot_t ballot,
                           shared_ptr<Marshallable> &cmd) {
  std::lock_guard<std::recursive_mutex> lock(mtx_);
#ifdef CURP_FULL_LOG_DEBUG
  Log_info("[CURP] Paxos OnCommit cmd<%d, %d>", SimpleRWCommand::GetCmdID(cmd).first, SimpleRWCommand::GetCmdID(cmd).second);
#endif
#ifdef LATENCY_DEBUG
  cli2oncommit_.append(SimpleRWCommand::GetCommandMsTimeElaps(cmd));
#endif
  // Log_info("[CURP] PaxosPlus OnCommit");
  Log_debug("multi-paxos scheduler decide for slot: %lx", slot_id);
  auto instance = GetInstance(slot_id);
  instance->committed_cmd_ = cmd;
  if (slot_id > max_committed_slot_) {
    max_committed_slot_ = slot_id;
  }
  verify(slot_id > max_executed_slot_);

#ifdef CURP_FULL_LOG_DEBUG
  Log_info("[CURP] About to CurpPreSkipFastpath cmd<%d, %d>", SimpleRWCommand::GetCmdID(instance->committed_cmd_).first, SimpleRWCommand::GetCmdID(instance->committed_cmd_).second);
#endif
  // CurpPreSkipFastpath(instance->committed_cmd_);

  // This prevents the log entry from being applied twice
  if (in_applying_logs_) {
    return;
  }
  in_applying_logs_ = true;
  for (slotid_t id = max_executed_slot_ + 1; id <= max_committed_slot_; id++) {
    auto next_instance = GetInstance(id);
    if (next_instance->committed_cmd_) {
#ifdef CURP_FULL_LOG_DEBUG
      Log_info("[CURP] About to CurpSkipFastpath cmd<%d, %d>", SimpleRWCommand::GetCmdID(next_instance->committed_cmd_).first, SimpleRWCommand::GetCmdID(next_instance->committed_cmd_).second);
#endif
      CurpSkipFastpath(curp_unique_original_cmd_id_++, next_instance->committed_cmd_);
      // WAN_WAIT
      app_next_(*next_instance->committed_cmd_);
      Log_debug("multi-paxos par:%d loc:%d executed slot %lx now", partition_id_, loc_id_, id);
      max_executed_slot_++;
      n_commit_++;
    } else {
      break;
    }
  }

  // TODO should support snapshot for freeing memory.
  // for now just free anything 1000 slots before.
  // TODO recover this
  // int i = min_active_slot_;
  // while (i + 1000 < max_executed_slot_) {
  //   logs_.erase(i);
  //   i++;
  // }
  // min_active_slot_ = i;
  in_applying_logs_ = false;
}

void PaxosPlusServer::Setup() {
  Log_info("Setup this=%p, this->loc_id_=%d, this->commo_==%p", 
        (void*)this, this->loc_id_, (void*)this->commo_);
}

} // namespace janus
