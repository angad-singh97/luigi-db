#include "../__dep__.h"
#include "coordinator.h"
#include "frame.h"
#include "commo.h"
#include "../bench/rw/procedure.h"
#include "../bench/rw/workload.h"
#include "../classic/tpc_command.h"

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
  Log_info("[copilot+] CopilotPlusCoordinator DoTxAsync");
}

void CopilotPlusCoordinator::Submit(shared_ptr<Marshallable> &cmd,
              const std::function<void()> &func,
              const std::function<void()> &exe_callback) {
  
  Log_info("[copilot+] enter Coordinator Submit");
  
  shared_ptr<TpcCommitCommand> tpc_cmd = dynamic_pointer_cast<TpcCommitCommand>(cmd);
  VecPieceData *cmd_cast = (VecPieceData*)(tpc_cmd->cmd_.get());
	shared_ptr<vector<shared_ptr<TxPieceData>>> sp_vec_piece = cmd_cast->sp_vec_piece_data_;
  shared_ptr<TxPieceData> vector0 = *(sp_vec_piece->begin());
  TxWorkspace tx_ws = vector0->input;
  std::map<int32_t, mdb::Value> kv_map = *(tx_ws.values_);
  key_t key = kv_map[0].get_i32();
  int32_t value = kv_map[1].get_i32();
  
  // key_t key = (*(*(((VecPieceData*)(dynamic_pointer_cast<TpcCommitCommand>(cmd)->cmd_.get()))->sp_vec_piece_data_->begin()))->input.values_)[0].get_i32();
  Log_info("[copilot+] key=%d value=%d", key, value);


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
  Log_info("[copilot+] enter Coordinator FrontRecover");
  auto sq_quorum = commo()->BroadcastFrontRecover(par_id_, accept_cmd_, max_response_.i, max_response_.j, max_response_.ballot);
  sq_quorum -> Wait();
  GotoNextPhase();
}

void CopilotPlusCoordinator::FrontCommit() {
  Log_info("[copilot+] enter Coordinator FrontRecover");
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