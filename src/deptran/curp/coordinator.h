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
  int32_t finish_countdown_;
  int32_t key_hotness_;
  bool fast_original_path_ = false;

  CoordinatorCurp(uint32_t coo_id,
                  int32_t benchmark,
                  ClientControlServiceImpl *ccsi,
                  uint32_t thread_id);
  ~CoordinatorCurp() {
    // Log_info("fastpath_count=%d, coordinatoraccept_count=%d, original_protocol_count=%d, cli2cli_50pct=%.2f ms, cli2cli_90pct=%.2f ms, cli2cli_99pct=%.2f ms",
    //   fastpath_count_, coordinatoraccept_count_, original_protocol_count_, cli2cli_->pct50(), cli2cli_->pct90(), cli2cli_->pct99());
  }
  void GotoNextPhase() override;
  void BroadcastDispatch();
  void QueryCoordinator();
  void OriginalProtocol();
};

} // namespace janus
