#pragma once

#include "../__dep__.h"
#include "../coordinator.h"
#include "../frame.h"

namespace janus {

class RaftCommo;
class RaftServer;
class CoordinatorRaft : public Coordinator {

 public:

  slotid_t slot_id_ = 0;
  slotid_t *slot_hint_ = nullptr;
  uint32_t n_replica_ = 0;

  RaftCommo *commo() {
    verify(commo_ != nullptr);
    return (RaftCommo *) commo_;
  }

  CoordinatorRaft(uint32_t coo_id,
                  int32_t benchmark,
                  ClientControlServiceImpl *ccsi,
                  uint32_t thread_id);

  ~CoordinatorRaft();

  uint32_t n_replica() {
    verify(n_replica_ > 0);
    return n_replica_;
  }

  void DoTxAsync(TxRequest &) override {}
  void Restart() override { verify(0); }

  void Submit(shared_ptr<Marshallable>& cmd,
              const std::function<void()>& commit_callback = [](){},
              const std::function<void()>& exe_callback = [](){});

};


}