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
  Log_info("CopilotPlusCoordinator created: this=%p, thread_id=%d", (void*)this, thread_id);
}

CopilotPlusCoordinator::~CopilotPlusCoordinator() {
}

void CopilotPlusCoordinator::DoTxAsync(TxRequest &) {

}

void CopilotPlusCoordinator::Submit(shared_ptr<Marshallable> &cmd,
              const std::function<void()> &func,
              const std::function<void()> &exe_callback) {
  Log_info("enter CopilotPlus Submit");
  auto sq_quorum = commo()->BroadcastSubmit(par_id_, cmd);
  return;
  // TODO: set time?
  sq_quorum -> Wait();
  fast_path_success_ = false;
  if (sq_quorum->FastYes()) {
    fast_path_success_ = true;
  } else if (sq_quorum->RecoverWithOpYes()) {
    accept_cmd_ = cmd;
  } else if (sq_quorum->RecoverWithoutOpYes()) {
    accept_cmd_ = empty_cmd_;
  } else {
    // number of reply < quorum size
    verify(0);
  }
  commit_callback_ = func;
  max_response_ = sq_quorum->GetMax();
  GotoNextPhase();
}

void CopilotPlusCoordinator::FrontRecover() {
  auto sq_quorum = commo()->BroadcastFrontRecover(par_id_, accept_cmd_, max_response_.i, max_response_.j, max_response_.ballot);
  sq_quorum -> Wait();
  GotoNextPhase();
}

void CopilotPlusCoordinator::FrontCommit() {
  auto sq_quorum = commo()->BroadcastFrontCommit(par_id_, accept_cmd_, max_response_.i, max_response_.j, max_response_.ballot);
  sq_quorum -> Wait();
  GotoNextPhase();
}

void CopilotPlusCoordinator::Restart() {

}

CopilotPlusCommo* CopilotPlusCoordinator::commo() {
  if (commo_ == nullptr) {
    Log_info("Coordinator=%p frame=%p", (void*)this, (void*)frame_);
    commo_ = frame_->CreateCommo(nullptr);
    commo_->loc_id_ = loc_id_;
  }
  Log_info("commo this=%p, this->loc_id_=%d, this->commo_==%p", (void*)this, this->loc_id_, (void*)this->commo_);
  verify(commo_);
  return (CopilotPlusCommo *)commo_;
}

void CopilotPlusCoordinator::GotoNextPhase() {
  switch (current_phase_) {
    case Phase::INIT_END:
      if (fast_path_success_) {
        current_phase_ = Phase::FRONT_COMMIT;
      } else {
        current_phase_ = Phase::FRONT_RECOVERY;
        FrontRecover();
      }
      break;
    case Phase::FRONT_RECOVERY:
      current_phase_ = Phase::FRONT_COMMIT;
      FrontCommit();
      break;
    case Phase::FRONT_COMMIT:
      break;
    default:
      break;
  }
}


}