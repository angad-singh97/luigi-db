#pragma once

#include "deptran/classic/coordinator.h"

namespace janus {

// This Coordinator should be on Client Side

class CoordinatorCurp : public CoordinatorClassic {
 public:
  enum Phase {INIT_END=0, DISPATCH=1, QUERY=2, ORIGIN=3};
  bool fast_path_success_{false};
  bool coordinator_success_{false};
  shared_ptr<VecPieceData> sp_vpd_; // cmd
  siteid_t curp_coo_id_ = -1;

  CoordinatorCurp(uint32_t coo_id,
                  int32_t benchmark,
                  ClientControlServiceImpl *ccsi,
                  uint32_t thread_id);
  void GotoNextPhase() override;
  void BroadcastDispatch();
  void QueryCoordinator();
  void OriginalProtocol();
};

} // namespace janus
