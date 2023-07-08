#pragma once

#include "../__dep__.h"
#include "../coordinator.h"
#include "../classic/tpc_command.h"
#include "../frame.h"
#include "../RW_command.h"
#include "../position.h"
#include "server.h"
#include <chrono>

namespace janus {

class CurpPlusCommo;
class CurpPlusServer;
class CoordinatorCurpPlus : public Coordinator {
 public:
  CurpPlusServer* sch_ = nullptr;
  uint32_t n_replica_ = 0;

  CurpPlusCommo *commo() {
    verify(commo_ != nullptr);
    return (CurpPlusCommo *) commo_;
  }
  
  CoordinatorCurpPlus(uint32_t coo_id,
                        int32_t benchmark,
                        ClientControlServiceImpl *ccsi,
                        uint32_t thread_id);

  // slotid_t slot_id_ = 0;
  // slotid_t *slot_hint_ = nullptr;

  // uint32_t n_replica() {
  //   verify(n_replica_ > 0);
  //   return n_replica_;
  // }

  // bool IsLeader() {
  //   return this->loc_id_ == 0;
  // }

  // slotid_t GetNextSlot() {
  //   verify(0);
  //   verify(slot_hint_ != nullptr);
  //   slot_id_ = (*slot_hint_)++;
  //   return 0;
  // }

  void DoTxAsync(TxRequest &req) override {}
  void Restart() override { verify(0); }

  // virtual void FastSubmit(shared_ptr<Marshallable>& cmd,
  //                         bool_t& accepted,
  //                         Position& pos,
  //                         value_t& result,
  //                         const std::function<void()>& commit_callback = [](){},
  //                         const std::function<void()>& exe_callback = [](){}) override;

  virtual void Submit(shared_ptr<Marshallable>& cmd,
                      const std::function<void()>& commit_callback = [](){},
                      const std::function<void()>& exe_callback = [](){}) override;

};

} //namespace janus
