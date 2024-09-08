#pragma once

#include "../__dep__.h"
#include "../coordinator.h"
#include "../frame.h"

namespace janus {

class RaftCommo;
class RaftServer;
class CoordinatorRaft : public Coordinator {
 public:
  RaftServer* sch_ = nullptr;
 private:
  enum Phase { INIT_END = 0, ACCEPT = 1, COMMIT = 2};

  RaftCommo *commo() {
    // TODO fix this.
    verify(commo_ != nullptr);
    return (RaftCommo *) commo_;
  }
  bool in_submission_ = false; // debug;
  bool in_append_entries = false; // debug
  uint64_t minIndex = 0;

 public:
  shared_ptr<Marshallable> cmd_{nullptr};
  CoordinatorRaft(uint32_t coo_id,
                        int32_t benchmark,
                        ClientControlServiceImpl *ccsi,
                        uint32_t thread_id);
  ~CoordinatorRaft();
  uint32_t n_replica_ = 0;   // TODO
  slotid_t slot_id_ = 0;
  slotid_t *slot_hint_ = nullptr;

  uint32_t n_replica() {
    verify(n_replica_ > 0);
    return n_replica_;
  }

  bool IsLeader() ;

  slotid_t GetNextSlot() {
    verify(0);
    verify(slot_hint_ != nullptr);
    slot_id_ = (*slot_hint_)++;
    return 0;
  }

  void DoTxAsync(TxRequest &req) override {}

  void Submit(shared_ptr<Marshallable> &cmd,
              const std::function<void()> &func = []() {},
              const std::function<void()> &exe_callback = []() {}) override;

  void AppendEntries();
  void Commit();

  void Reset() override {}
  void Restart() override { verify(0); }

  void GotoNextPhase();
};

} //namespace janus
