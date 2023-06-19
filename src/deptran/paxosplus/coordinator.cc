
#include "../__dep__.h"
#include "../constants.h"
#include "coordinator.h"
#include "commo.h"

namespace janus {

CoordinatorMultiPaxosPlus::CoordinatorMultiPaxosPlus(uint32_t coo_id,
                                             int32_t benchmark,
                                             ClientControlServiceImpl* ccsi,
                                             uint32_t thread_id)
    : Coordinator(coo_id, benchmark, ccsi, thread_id) {
}

void CoordinatorMultiPaxosPlus::Forward(){
  verify(!in_forward);
  in_forward=true;
  auto follower_id = frame_->site_info_->id;
  auto sp_quorum = commo()->SendForward(par_id_, follower_id, dep_id_, cmd_);

  sp_quorum->Wait();
  Log_info("Follower done waiting");
  sp_quorum->log();
}
                                    

void CoordinatorMultiPaxosPlus::Submit(shared_ptr<Marshallable>& cmd,
                                   const function<void()>& func,
                                   const function<void()>& exe_callback) {
  if (!IsLeader()) {
    //change back to fatal
    Log_info("i am not the leader; site %d; locale %d",
              frame_->site_info_->id, loc_id_);
  }

  std::lock_guard<std::recursive_mutex> lock(mtx_);
  verify(!in_submission_);
  verify(cmd_ == nullptr);
//  verify(cmd.self_cmd_ != nullptr);
  in_submission_ = true;
  cmd_ = cmd;
  verify(cmd_->kind_ != MarshallDeputy::UNKNOWN);
  commit_callback_ = func;
  GotoNextPhase();
}

/***************************************PLUS Begin***********************************************************/
void CoordinatorMultiPaxosPlus::FastSubmit(shared_ptr<Marshallable>& cmd,
                                        bool_t& accepted,
                                        Position& pos,
                                        value_t& result,
                                        const std::function<void()> &func,
                                        const std::function<void()> &exe_callback) {
  std::lock_guard<std::recursive_mutex> lock(mtx_);
  Log_info("[copilot+] [4+] enter CopilotPlusCoordinator::FastSubmit");

  shared_ptr<SimpleRWCommand> parsed_cmd_ = make_shared<SimpleRWCommand>(cmd);
  key_t key = dynamic_pointer_cast<SimpleRWCommand>(parsed_cmd_)->key_;
  bool_t fast_path_validation = sch_->check_fast_path_validation(key);
  if (!fast_path_validation) {
    Log_info("[copilot+] branch 1");
    accepted = false;
    pos.set(0, -1);
    result = 0;
  } else {
    accepted = true;
    if (parsed_cmd_->type_ == SimpleRWCommand::CmdType::Read) {
      pos.set(0, -1);
      result = sch_->read(key);
    } else if (parsed_cmd_->type_ == SimpleRWCommand::CmdType::Write) {
      slotid_t new_slot_pos = sch_->append_cmd(key, cmd);
      pos.set(0, new_slot_pos);
      result = 1;
    } else {
      verify(0);
    }
  }
  shared_ptr<MultiPaxosPlusForwardQuorumEvent> sq_quorum = commo()->ForwardResultToCoordinator(par_id_, cmd, accepted, pos);
  // TODO
  commit_callback_ = func;
  commit_callback_();
}



/***************************************PLUS End***********************************************************/

ballot_t CoordinatorMultiPaxosPlus::PickBallot() {
  return curr_ballot_ + 1;
}

void CoordinatorMultiPaxosPlus::Prepare() {
  std::lock_guard<std::recursive_mutex> lock(mtx_);
  verify(0); // for debug;
  verify(!in_prepare_);
  in_prepare_ = true;
  curr_ballot_ = PickBallot();
  verify(slot_id_ > 0);
  //rpc_event->add_dep(commo()->LeaderProxyForPartition(par_id_).first);
  //rpc_event->log();
  Log_debug("multi-paxos coordinator broadcasts prepare, "
                "par_id_: %lx, slot_id: %llx",
            par_id_,
            slot_id_);
  verify(n_prepare_ack_ == 0);
  int n_replica = Config::GetConfig()->GetPartitionSize(par_id_);
  auto sp_quorum = commo()->BroadcastPrepare(par_id_, slot_id_, curr_ballot_);
  auto start = chrono::steady_clock::now();
  Log_info("Time before Wait() is: %d", chrono::duration_cast<chrono::milliseconds>(start.time_since_epoch()).count());
  sp_quorum->Wait();
  auto end = chrono::steady_clock::now();

  auto duration = chrono::duration_cast<chrono::milliseconds>(end-start);
  Log_info("Duration of Wait() in Prepare() is: %d", duration.count());
  sp_quorum->log();
  if (sp_quorum->Yes()) {
    verify(!sp_quorum->HasAcceptedValue());
    // TODO use the previously accepted value.

  } else if (sp_quorum->No()) {
    // TODO restart prepare?
    verify(0);
  } else {
    // TODO timeout
    verify(0);
  }
//  commo()->BroadcastPrepare(par_id_,
//                            slot_id_,
//                            curr_ballot_,
//                            std::bind(&CoordinatorMultiPaxosPlus::PrepareAck,
//                                      this,
//                                      phase_,
//                                      std::placeholders::_1));
//}
//
//void CoordinatorMultiPaxosPlus::PrepareAck(phase_t phase, Future* fu) {
//  std::lock_guard<std::recursive_mutex> lock(mtx_);
//  if (phase_ != phase) return;
//  ballot_t max_ballot;
//  fu->get_reply() >> max_ballot;
//  if (max_ballot == curr_ballot_) {
//    n_prepare_ack_++;
//    verify(n_prepare_ack_ <= n_replica_);
//    if (n_prepare_ack_ >= GetQuorum()) {
//      GotoNextPhase();
//    }
//  } else {
//    if (max_ballot > curr_ballot_) {
//      curr_ballot_ = max_ballot + 1;
//      Log_debug("%s: saw greater ballot increment to %d",
//                __FUNCTION__, curr_ballot_);
//      phase_ = Phase::INIT_END;
//      GotoNextPhase();
//    } else {
////       max_ballot < curr_ballot ignore
//    }
//  }
}

void CoordinatorMultiPaxosPlus::Accept() {
  std::lock_guard<std::recursive_mutex> lock(mtx_);
  verify(!in_accept);
  in_accept = true;
  Log_debug("multi-paxos coordinator broadcasts accept, "
                "par_id_: %lx, slot_id: %llx",
            par_id_, slot_id_);
  auto start = chrono::system_clock::now();
  auto sp_quorum = commo()->BroadcastAccept(par_id_, slot_id_, curr_ballot_, cmd_);
  sp_quorum->id_ = dep_id_;
	Log_info("current coroutine's dep_id: %d", Coroutine::CurrentCoroutine()->dep_id_);
  //Log_info("Accept(): %d", dep_id_);

  sp_quorum->Wait();
  auto end = chrono::system_clock::now();
  auto duration = chrono::duration_cast<chrono::microseconds>(end-start);
  //auto duration_ready = chrono::duration_cast<chrono::microseconds>(end-sp_quorum->ready_time);
  Log_info("Duration of Wait() in Accept() is: %d", duration.count());
  //Log_info("Duration after Ready to end of Wait() is: %d", duration_ready.count());
  sp_quorum->log();
  if (sp_quorum->Yes()) {
    committed_ = true;
  } else if (sp_quorum->No()) {
    // TODO process the case: failed to get a majority.
    verify(0);
  } else {
    // TODO process timeout.
    verify(0);
  }
//  commo()->BroadcastAccept(par_id_,
//                           slot_id_,
//                           curr_ballot_,
//                           cmd_,
//                           std::bind(&CoordinatorMultiPaxosPlus::AcceptAck,
//                                     this,
//                                     phase_,
//                                     std::placeholders::_1));
//}
//
//void CoordinatorMultiPaxosPlus::AcceptAck(phase_t phase, Future* fu) {
//  std::lock_guard<std::recursive_mutex> lock(mtx_);
//  if (phase_ > phase) return;
//  ballot_t max_ballot;
//  fu->get_reply() >> max_ballot;
//  if (max_ballot == curr_ballot_) {
//    n_finish_ack_++;
//    if (n_finish_ack_ >= GetQuorum()) {
//      committed_ = true;
//      GotoNextPhase();
//    }
//  } else {
//    if (max_ballot > curr_ballot_) {
//      curr_ballot_ = max_ballot + 1;
//      Log_debug("%s: saw greater ballot increment to %d",
//                __FUNCTION__, curr_ballot_);
//      phase_ = Phase::INIT_END;
//      GotoNextPhase();
//    } else {
//      // max_ballot < curr_ballot ignore
//    }
//  }
}

void CoordinatorMultiPaxosPlus::Commit() {
  std::lock_guard<std::recursive_mutex> lock(mtx_);
  commit_callback_();
  Log_debug("multi-paxos broadcast commit for partition: %d, slot %d",
            (int) par_id_, (int) slot_id_);
  commo()->BroadcastDecide(par_id_, slot_id_, curr_ballot_, cmd_);
  verify(phase_ == Phase::COMMIT);
  GotoNextPhase();
}

void CoordinatorMultiPaxosPlus::GotoNextPhase() {
  int n_phase = 4;
  int current_phase = phase_ % n_phase;
  //Log_info("Current phase is %d", current_phase);
  phase_++;
  switch (current_phase) {
    case Phase::INIT_END:
      if (IsLeader()) {
        phase_++; // skip prepare phase for "leader"
        verify(phase_ % n_phase == Phase::ACCEPT);
        Accept();
        phase_++;
        verify(phase_ % n_phase == Phase::COMMIT);
      } else {
        // TODO
        verify(0);
        Log_info("The local id is %d", this->loc_id_);
        //Forward();
        //Log_info("Follower logic");
        //For now, do nothing
        //
        //Next steps: Find the leader, call submit, wait for the reply
      }
    case Phase::ACCEPT:
      verify(phase_ % n_phase == Phase::COMMIT);
      if (committed_) {
        Commit();
      }
      else{
        verify(0);
      }
      break;
    case Phase::PREPARE:
      verify(phase_ % n_phase == Phase::ACCEPT);
      Accept();
      break;
    case Phase::COMMIT:
      // do nothing.
      break;
    default:
      verify(0);
  }
}

} // namespace janus
