#include "../__dep__.h"
#include "coordinator.h"
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

}

void CopilotPlusCoordinator::Submit(shared_ptr<Marshallable> &cmd,
              const std::function<void()> &func,
              const std::function<void()> &exe_callback) {
  auto sq_quorum = commo()->BroadcastSubmit(par_id_, cmd);
  // TODO: set time?
  sq_quorum -> Wait();
  fast_path_ = false;
  if (sq_quorum->FastYes()) {
    fast_path_ = true;
  } else if (sq_quorum->FastNo()) {

  } else {

  }
  GotoNextPhase();
}

void CopilotPlusCoordinator::Restart() {

}

CopilotPlusCommo* CopilotPlusCoordinator::commo() {
  verify(commo_);
  return (CopilotPlusCommo *)commo_;
}

void CopilotPlusCoordinator::GotoNextPhase() {
  phase_t current_phase = phase_++;
  switch (current_phase) {
    case INIT_END:
      break;
  }
}


}
