
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

void CoordinatorCurpPlus::Submit(shared_ptr<Marshallable>& cmd,
                                  const std::function<void()>& commit_callback,
                                  const std::function<void()>& exe_callback) {
  std::lock_guard<std::recursive_mutex> lock(mtx_);

  shared_ptr<CurpDispatchQuorumEvent> sq_quorum = commo()->CurpBroadcastDispatch(cmd);
  commit_callback_ = commit_callback;

  sq_quorum->Wait();
  // Log_info("[CURP] After quorum");
  if (sq_quorum->FastYes()) {
    // Fastpath Success
    // Log_info("[CURP] Fastpath Success");
    commit_callback_();
  } else if (sq_quorum->timeouted_) {
    // Fastpath timeout
    // Log_info("[CURP] Fastpath Fail");
    auto wait_quorum = commo()->DirectCurpBroadcastWaitCommit(cmd, sq_quorum->GetCooId());
    wait_quorum->Wait();
    if (wait_quorum->Yes()) // 0 fail 1 success
      commit_callback_();
    else
      {/*TODO*/}
  }
  
}

} // namespace janus
