#pragma once

#include "deptran/classic/coordinator.h"

namespace janus {

// This Coordinator should be on Client Side

class CoordinatorRule : public CoordinatorClassic {
 public:
  enum Phase {INIT_END=0, DISPATCHED=1, WAITING_ORIGIN=2};
  bool fast_path_success_{false};
  bool coordinator_success_{false};
  shared_ptr<VecPieceData> sp_vpd_; // cmd

  double margin_success_rate_;

  value_t result_;

  CoordinatorRule(uint32_t coo_id,
                  int32_t benchmark,
                  ClientControlServiceImpl *ccsi,
                  uint32_t thread_id);
  ~CoordinatorRule() {
    // Log_info("fastpath_count=%d, coordinatoraccept_count=%d, original_protocol_count=%d, cli2cli_50pct=%.2f ms, cli2cli_90pct=%.2f ms, cli2cli_99pct=%.2f ms",
    //   fastpath_count_, coordinatoraccept_count_, original_protocol_count_, cli2cli_->pct50(), cli2cli_->pct90(), cli2cli_->pct99());
  }
  void GotoNextPhase() override;
  void BroadcastRuleSpeculativeExecute(int cmd_ver);
  void DispatchAsync() override;
};

} // namespace janus
