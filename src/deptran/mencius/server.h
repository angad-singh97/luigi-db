#pragma once

#include "../__dep__.h"
#include "../constants.h"
#include "../scheduler.h"
#include "../classic/tpc_command.h"
#include "../classic/tx.h"
#include "coordinator.h"
#include <chrono>
#include <ctime>

namespace janus {
class Command;
class CmdData;

struct MenciusData {
  ballot_t max_ballot_seen_ = 0;
  ballot_t max_ballot_suggested_ = 0;
  shared_ptr<Marshallable> accepted_cmd_{nullptr};
  shared_ptr<Marshallable> committed_cmd_{nullptr};
};

class MenciusServer : public TxLogServer {
 public:
  // ----min_active <= max_executed <= max_committed---
  slotid_t min_active_slot_ = 0; // anything before (lt) this slot is freed
  slotid_t max_executed_slot_ = 0;
  slotid_t max_committed_slot_ = 0;
  map<slotid_t, shared_ptr<MenciusData>> logs_{};
  int n_prepare_ = 0;
  int n_suggest_ = 0;
  int n_commit_ = 0;
  bool in_applying_logs_{false};

  ~MenciusServer() {
    Log_info("site par %d, loc %d: prepare %d, accept %d, commit %d", partition_id_, loc_id_, n_prepare_, n_suggest_, n_commit_);
  }

  shared_ptr<MenciusData> GetInstance(slotid_t id) {
    verify(id >= min_active_slot_);
    auto& sp_instance = logs_[id];
    // for (auto const& x : logs_) {
    //   Log_info("GetInstance slot_id: %llu on loc_id_:%d [logs]", x.first, this->loc_id_);
    // }

    if(!sp_instance)
      sp_instance = std::make_shared<MenciusData>();
    return sp_instance;
  }

  void OnPrepare(slotid_t slot_id,
                 ballot_t ballot,
                 ballot_t *max_ballot,
                 uint64_t* coro_id,
                 const function<void()> &cb);

  void OnSuggest(const slotid_t slot_id,
		const uint64_t time,
                const ballot_t ballot,
                shared_ptr<Marshallable> &cmd,
                ballot_t *max_ballot,
                uint64_t* coro_id,
                const function<void()> &cb);

  void OnCommit(const slotid_t slot_id,
                const ballot_t ballot,
                shared_ptr<Marshallable> &cmd);

  void Setup();

  virtual bool HandleConflicts(Tx& dtxn,
                               innid_t inn_id,
                               vector<string>& conflicts) {
    verify(0);
  };
};
} // namespace janus
