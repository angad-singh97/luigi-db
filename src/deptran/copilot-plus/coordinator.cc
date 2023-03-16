#include "../__dep__.h"
#include "coordinator.h"
#include "frame.h"
#include "commo.h"

namespace janus {

CopilotPlusCoordinator::CopilotPlusCoordinator(uint32_t coo_id,
              int benchmark,
              ClientControlServiceImpl *ccsi = NULL,
              uint32_t thread_id = 0) 
  :Coordinator(coo_id, benchmark, ccsi, thread_id){
}

CopilotPlusCoordinator::~CopilotPlusCoordinator() {
}

void CopilotPlusCoordinator::DoTxAsync(TxRequest &) {
  //Log_info("[copilot+] CopilotPlusCoordinator DoTxAsync");
}

void CopilotPlusCoordinator::Submit(shared_ptr<Marshallable> &cmd,
              const std::function<void()> &func,
              const std::function<void()> &exe_callback) {
  received_cmd_ = cmd;
  parsed_cmd_ = make_shared<SimpleRWCommand>(cmd);
  //Log_info("[copilot+] enter Coordinator Submit %s", parsed_cmd_->cmd_to_string().c_str());

  auto sq_quorum = commo()->BroadcastSubmit(par_id_, slot_id_, dynamic_pointer_cast<Marshallable>(cmd));
  // TODO: set time?
  sq_quorum -> Wait(2000);
  //Log_info("[copilot+] received reply %s", parsed_cmd_->cmd_to_string().c_str());
  fast_path_success_ = false;
  if (sq_quorum->FastYes()) {
    fast_path_success_ = true;
    //Log_info("[copilot+] accept_cmd_ is received_cmd_");
  } else if (sq_quorum->RecoverWithOpYes()) {
    //Log_info("[copilot+] accept_cmd_ is received_cmd_");
  } else if (sq_quorum->RecoverWithoutOpYes()) {
    //Log_info("[copilot+] accept_cmd_ is empty_cmd_");
    commit_no_op_ = true;
  } else {
    // number of reply < quorum size
    verify(0);
  }
  commit_callback_ = func;
  max_response_ = sq_quorum->GetMax();
  //Log_info("[copilot+] exit Coordinator Submit %s", parsed_cmd_->cmd_to_string().c_str());
  GotoNextPhase();
}

void CopilotPlusCoordinator::FrontRecover() {
  //Log_info("[copilot+] enter Coordinator FrontRecover %s", parsed_cmd_->cmd_to_string().c_str());
  auto sq_quorum = commo()->BroadcastFrontRecover(par_id_, slot_id_, received_cmd_, commit_no_op_, max_response_.i, max_response_.j, max_response_.ballot);
  sq_quorum -> Wait();
  //Log_info("[copilot+] exit Coordinator FrontRecover %s", parsed_cmd_->cmd_to_string().c_str());
  GotoNextPhase();
}

void CopilotPlusCoordinator::FrontCommit() {
  //Log_info("[copilot+] enter Coordinator FrontCommit %s", parsed_cmd_->cmd_to_string().c_str());
  commit_callback_();
  //Log_info("[copilot+] Copilot coordinator broadcast FrontCommit for partition: %d", (int) par_id_);
  auto sq_quorum = commo()->BroadcastFrontCommit(par_id_, slot_id_, received_cmd_, commit_no_op_, max_response_.i, max_response_.j, max_response_.ballot);
  sq_quorum -> Wait();
  //Log_info("[copilot+] exit Coordinator FrontCommit %s", parsed_cmd_->cmd_to_string().c_str());
  GotoNextPhase();
}

void CopilotPlusCoordinator::Restart() {

}

CopilotPlusCommo* CopilotPlusCoordinator::commo() {
  if (commo_ == nullptr) {
    //Log_info("[copilot+] Coordinator=%p frame=%p", (void*)this, (void*)frame_);
    commo_ = frame_->CreateCommo(nullptr);
    commo_->loc_id_ = loc_id_;
  }
  //Log_info("[copilot+] commo this=%p, this->loc_id_=%d, this->commo_==%p", (void*)this, this->loc_id_, (void*)this->commo_);
  verify(commo_);
  return (CopilotPlusCommo *)commo_;
}

void CopilotPlusCoordinator::GotoNextPhase() {
  //Log_info("[copilot+] enter GotoNextPhase");
  switch (current_phase_) {
    case Phase::INIT_END:
      if (fast_path_success_) {
        //Log_info("[copilot+] FRONT_COMMIT");
        current_phase_ = Phase::FRONT_COMMIT;
        FrontCommit();
      } else {
        //Log_info("[copilot+] FRONT_RECOVERY");
        current_phase_ = Phase::FRONT_RECOVERY;
        FrontRecover();
      }
      break;
    case Phase::FRONT_RECOVERY:
      //Log_info("[copilot+] FRONT_COMMIT");
      current_phase_ = Phase::FRONT_COMMIT;
      FrontCommit();
      break;
    case Phase::FRONT_COMMIT:
      break;
    default:
      break;
  }
  //Log_info("[copilot+] exit GotoNextPhase");
}


}