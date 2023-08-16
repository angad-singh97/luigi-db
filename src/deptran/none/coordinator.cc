
#include "coordinator.h"
#include "frame.h"
#include "benchmark_control_rpc.h"

namespace janus {

/** thread safe */

void CoordinatorNone::GotoNextPhase() {
  Log_debug("GoToNextPhase on client side");
  int n_phase = 2;
  switch (phase_++ % n_phase) {
    case Phase::INIT_END:
      dispatch_time_ = SimpleRWCommand::GetCurrentMsTime();
      DispatchAsync();
      verify(phase_ % n_phase == Phase::DISPATCH);
      break;
    case Phase::DISPATCH:
      committed_ = true;
      verify(phase_ % n_phase == Phase::INIT_END);
      cli2cli_->append(SimpleRWCommand::GetCurrentMsTime() - dispatch_time_);
      End();
      break;
    default:
      verify(0);
  }
}

} // namespace janus
