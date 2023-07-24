#pragma once

#include "../__dep__.h"
#include "../constants.h"
#include "../scheduler.h"
#include "../classic/tpc_command.h"
#include "../classic/tx.h"
#include "coordinator.h"
#include "commo.h"
#include <chrono>
#include <ctime>
#include "../curpplus/server.h"

namespace janus {
class Command;
class CmdData;

struct PaxosPlusData {
  ballot_t max_ballot_seen_ = 0;
  ballot_t max_ballot_accepted_ = 0;
  shared_ptr<Marshallable> accepted_cmd_{nullptr};
  shared_ptr<Marshallable> committed_cmd_{nullptr};
};

class PaxosPlusServer : public TxLogServer {
 public:
  // ----min_active <= max_executed <= max_committed---
  slotid_t min_active_slot_ = 0; // anything before (lt) this slot is freed
  slotid_t max_executed_slot_ = 0;
  slotid_t max_committed_slot_ = 0;
  map<slotid_t, shared_ptr<PaxosPlusData>> logs_{};
  int n_prepare_ = 0;
  int n_accept_ = 0;
  int n_commit_ = 0;
  bool in_applying_logs_{false};

  PaxosPlusServer() {
  }

  ~PaxosPlusServer() {
    Log_info("site par %d, loc %d: prepare %d, accept %d, commit %d", partition_id_, loc_id_, n_prepare_, n_accept_, n_commit_);
  }

  shared_ptr<PaxosPlusData> GetInstance(slotid_t id) {
    verify(id >= min_active_slot_);
    auto& sp_instance = logs_[id];
    if(!sp_instance)
      sp_instance = std::make_shared<PaxosPlusData>();
    return sp_instance;
  }

  void OnForward(shared_ptr<Marshallable> &cmd,
                 uint64_t dep_id,
                 uint64_t* coro_id,
                 const function<void()> &cb);

  void OnPrepare(slotid_t slot_id,
                 ballot_t ballot,
                 ballot_t *max_ballot,
                 uint64_t* coro_id,
                 const function<void()> &cb);

  void OnAccept(const slotid_t slot_id,
		            const uint64_t time,
                const ballot_t ballot,
                shared_ptr<Marshallable> &cmd,
                ballot_t *max_ballot,
                uint64_t* coro_id,
                const function<void()> &cb);

  void OnCommit(const slotid_t slot_id,
                const ballot_t ballot,
                shared_ptr<Marshallable> &cmd);

  void OnOriginalSubmit(shared_ptr<Marshallable> &cmd,
                        const rrr::i64& dep_id,
                        bool_t* slow,
                        const function<void()> &cb);

  void Setup();

  virtual bool HandleConflicts(Tx& dtxn,
                               innid_t inn_id,
                               vector<string>& conflicts) {
    verify(0);
  };

  // below are about CURP

  MultiPaxosPlusCommo *commo() {
    // TODO fix this.
    verify(commo_ != nullptr);
    return (MultiPaxosPlusCommo *) commo_;
  }

  bool TryAssignGlobalID(slotid_t local_id);
};

} // namespace janus
