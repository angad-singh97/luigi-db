#pragma once

#include "../__dep__.h"
#include "../coordinator.h"
#include "server.h"
#include "commo.h"
#include "RW_command.h"

namespace janus {

class CopilotPlusCoordinator: public Coordinator {
 public:
  enum Phase {INIT_END, FRONT_RECOVERY, FRONT_COMMIT};
 private:
  Phase current_phase_ = INIT_END;
  bool_t fast_path_success_ = false;
  bool_t commit_no_op_ = false;
  shared_ptr<Marshallable> received_cmd_ = nullptr;
  shared_ptr<SimpleRWCommand> parsed_cmd_ = nullptr;
  CopilotPlusSubmitQuorumEvent::ResponsePack max_response_;
 public:
  uint32_t n_replica_ = 0;
  slotid_t slot_id_ = 0;
  slotid_t *slot_hint_ = nullptr;
  CopilotPlusCoordinator(uint32_t coo_id,
              int benchmark,
              ClientControlServiceImpl *ccsi,
              uint32_t thread_id);
  virtual ~CopilotPlusCoordinator();
  void DoTxAsync(TxRequest &) override;


  void Submit(shared_ptr<Marshallable> &cmd,
              const std::function<void()> &func = []() {},
              const std::function<void()> &exe_callback = []() {}) override;
  
  void FrontRecover();

  void FrontCommit();

  void Restart() override;
 private:
  CopilotPlusCommo* commo();
  void GotoNextPhase();
};



}