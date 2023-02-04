#pragma once

#include "../__dep__.h"
#include "../coordinator.h"
#include "server.h"

namespace janus {

class CopilotPlusCoordinator: public Coordinator {
  int current_phase_ = 0;
  CopilotPlusServer *svr = nullptr;
 public:
  enum Phase: int {};
  CopilotPlusCoordinator(uint32_t coo_id,
              int benchmark,
              ClientControlServiceImpl *ccsi,
              uint32_t thread_id);

  virtual ~CopilotPlusCoordinator();
  void DoTxAsync(TxRequest &) override;
  void Restart() override;
};



}