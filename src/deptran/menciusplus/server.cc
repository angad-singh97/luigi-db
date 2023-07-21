

#include "server.h"
// #include "paxos_worker.h"
#include "exec.h"
#include "../RW_command.h"

namespace janus {

void MenciusPlusServer::OnPrepare(slotid_t slot_id,
                            ballot_t ballot,
                            ballot_t *max_ballot,
                            uint64_t* coro_id,
                            const function<void()> &cb) {

  std::lock_guard<std::recursive_mutex> lock(mtx_);
  Log_debug("mencius scheduler receives prepare for slot_id: %llx",
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
  cb();
}


void MenciusPlusServer::OnSuggest(const slotid_t slot_id,
		                       const uint64_t time,
                           const ballot_t ballot,
                           const uint64_t sender,
                           const std::vector<uint64_t>& skip_commits, 
                           const std::vector<uint64_t>& skip_potentials,
                           shared_ptr<Marshallable> &cmd,
                           ballot_t *max_ballot,
                           uint64_t* coro_id,
                           const function<void()> &cb) {
  std::lock_guard<std::recursive_mutex> lock(mtx_);
  //Log_info("mencius scheduler suggest for slot_id: %llu", slot_id);
  auto instance = GetInstance(slot_id);

  //TODO: might need to optimize this. we can vote yes on duplicates at least for now
  //verify(instance->max_ballot_suggested_ < ballot);
  
  if (instance->max_ballot_seen_ <= ballot) {
    instance->max_ballot_seen_ = ballot;
    instance->max_ballot_suggested_ = ballot;
  } else {
    // TODO
    verify(0);
  }

  *coro_id = Coroutine::CurrentCoroutine()->id;
  *max_ballot = instance->max_ballot_seen_;
  n_suggest_++;
  cb();
}

void MenciusPlusServer::OnCommit(const slotid_t slot_id,
                           const ballot_t ballot,
                           shared_ptr<Marshallable> &cmd,
                           bool is_skip) {
  std::lock_guard<std::recursive_mutex> lock(mtx_);
  //Log_info("mencius scheduler decide for slot: %d on loc_id_:%d", slot_id, this->loc_id_);
  auto instance = GetInstance(slot_id);
  instance->committed_cmd_ = cmd;
  instance->is_skip = true;
  if (instance->is_skip){
    instance->committed_cmd_->kind_ = MarshallDeputy::CMD_TPC_COMMIT;
  }
  if (slot_id > max_committed_slot_) {
    max_committed_slot_ = slot_id;
  }
  verify(slot_id > max_executed_slot_);
  // This prevents the log entry from being applied twice
  if (in_applying_logs_) {
    return;
  }
  in_applying_logs_ = true;
  for (slotid_t id = max_executed_slot_ + 1; id <= max_committed_slot_; id++) {
    auto next_instance = GetInstance(id);
    if (next_instance->committed_cmd_ && TryAssignGlobalID(id)) {
      if (executed_slots_[id]!=1){
        app_next_(*next_instance->committed_cmd_);
        executed_slots_.erase(id);
      }
        
      Log_debug("mencius par:%d loc:%d executed slot %lx now", partition_id_, loc_id_, id);
      max_executed_slot_++;
      n_commit_++;
    } else {
      break;
    }
  }

  // apply the entry out of order if there is no conflict
  for (slotid_t id = max_executed_slot_ + 1; id <= max_committed_slot_; id++) {
    auto next_instance = GetInstance(id);
    if (next_instance->committed_cmd_) {
      SimpleRWCommand parsed_cmd = SimpleRWCommand(next_instance->committed_cmd_);
      if (uncommitted_keys_[parsed_cmd.key_]==0){
        executed_slots_[id]=1;
        app_next_(*next_instance->committed_cmd_);
      }
    }
  }

  // TODO should support snapshot for freeing memory.
  // for now just free anything 1000 slots before.
  int i = min_active_slot_;
  std::lock_guard<std::mutex> guard(g_mutex);
  {
    while (i + 1000 < max_executed_slot_) {
      logs_.erase(i);
      i++;
    }
  }
  min_active_slot_ = i;
  in_applying_logs_ = false;
}

void MenciusPlusServer::Setup() {
  Log_info("Setup this=%p, this->loc_id_=%d, this->commo_==%p", 
        (void*)this, this->loc_id_, (void*)this->commo_);
}

bool MenciusPlusServer::TryAssignGlobalID(slotid_t local_id) {
  shared_ptr<Marshallable> to_assign_cmd = GetInstance(local_id)->committed_cmd_;
  key_t key = get_key_from_marshallable(to_assign_cmd);
  slotid_t global_id = OriginalProtocolApplyForNewGlobalID(key);
  return global_id != 0;
}

} // namespace janus
