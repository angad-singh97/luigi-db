#pragma once

#include "../__dep__.h"
#include "../coordinator.h"
#include "server.h"
#include "commo.h"

namespace janus {

class CopilotPlusCoordinator: public Coordinator {
 public:
  enum Phase {INIT_END};
 private:
  Phase current_phase_ = INIT_END;
  bool fast_path_ = false;
 public:
  CopilotPlusCoordinator(uint32_t coo_id,
              int benchmark,
              ClientControlServiceImpl *ccsi,
              uint32_t thread_id);
  virtual ~CopilotPlusCoordinator();
  void DoTxAsync(TxRequest &) override;


  void Submit(shared_ptr<Marshallable> &cmd,
              const std::function<void()> &func = []() {},
              const std::function<void()> &exe_callback = []() {}) override;
  


  void Restart() override;
 private:
  CopilotPlusCommo* commo();
  void GotoNextPhase();
};



}