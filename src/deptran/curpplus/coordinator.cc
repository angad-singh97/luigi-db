
#include "../__dep__.h"
#include "../constants.h"
#include "coordinator.h"
#include "commo.h"

namespace janus {

CoordinatorCurpPlus::CoordinatorCurpPlus(uint32_t coo_id,
                                             int32_t benchmark,
                                             ClientControlServiceImpl* ccsi,
                                             uint32_t thread_id)
    : Coordinator(coo_id, benchmark, ccsi, thread_id) {
  // Log_info("[CURP] Coordinator created with coo_id=%d thread_id=%d", coo_id, thread_id);
}

// void CoordinatorCurpPlus::Submit(shared_ptr<Marshallable>& cmd,
//                                   const std::function<void()>& commit_callback,
//                                   const std::function<void()>& exe_callback) {
//   std::lock_guard<std::recursive_mutex> lock(mtx_);

//   // Log_info("[CURP] enter CoordinatorCurpPlus::Submit!!!!!!!!!!!!!!!!!!!!");
//   // verify(sch_ != nullptr);
//   // Log_info("[CURP] the coresponding server is %d %d", ((CurpPlusServer*) sch_)->loc_id_, ((CurpPlusServer*) sch_)->site_id_);

//   // return;

//   shared_ptr<CurpDispatchQuorumEvent> sq_quorum = commo()->CurpBroadcastDispatch(cmd);
//   commit_callback_ = commit_callback;

//   sq_quorum->Wait();
//   // Log_info("[CURP] After quorum");
//   if (sq_quorum->FastYes()) {
//     // Fastpath Success
//     // Log_info("[CURP] Fastpath Success");
//     // [CURP] TODO: maybe some problem
//     commit_callback_();
//   } else if (sq_quorum->timeouted_) {
//     // Fastpath timeout
//     Log_info("[CURP] Fastpath Fail");
//     auto wait_quorum = commo()->CurpBroadcastWaitCommit(cmd, sq_quorum->GetCooId());
//     wait_quorum->Wait();
//     if (wait_quorum->Yes()) // 0 fail 1 success
//       commit_callback_();
//     else
//       {/*TODO*/}
//   }
// }

// void CoordinatorClassic::CurpBroadcastDispatch(shared_ptr<vector<shared_ptr<TxPieceData>>> sp_vec_piece) {
//   TxnOutput outputs;
//   siteid_t leader;
//   auto sq_quorum = commo()->CurpBroadcastDispatch(sp_vec_piece);
//   sq_quorum->Wait();
//   Log_info("[copilot+] After quorum");
//   if (sq_quorum->FastYes()) {
//     // Fastpath Success
//     Log_info("[copilot+] Fastpath Success");
//     CoordinatorClassic::DispatchAck(phase_, SUCCESS, outputs);
//   } else if (sq_quorum->timeouted_) {
//     // Fastpath timeout
//     Log_info("[copilot+] Fastpath Fail");
//     auto wait_quorum = commo()->DirectCurpBroadcastWaitCommit(sp_vec_piece, sq_quorum->GetCooId());
//     wait_quorum->Wait();
//     if (wait_quorum->Yes()) // 0 fail 1 success
//       CoordinatorClassic::DispatchAck(phase_, SUCCESS, outputs);
//     else
//       CoordinatorClassic::DispatchAck(phase_, REJECT, outputs);
//   }
// }

} // namespace janus
