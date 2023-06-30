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

namespace janus {

struct PaxosPlusData {
  enum PaxosPlusStatus {
    init = 0,
    fastAccepted = 1,
    prepared = 2,
    accepted = 3,
    committed = 4
  };
  PaxosPlusStatus status_ = PaxosPlusStatus::init;
  ballot_t max_ballot_seen_ = 0;
  ballot_t max_ballot_accepted_ = 0;
  shared_ptr<Marshallable> fast_accepted_cmd_{nullptr};
  shared_ptr<Marshallable> accepted_cmd_{nullptr};
  shared_ptr<Marshallable> committed_cmd_{nullptr};
  
  shared_ptr<Marshallable> last_accepted_{nullptr};
  PaxosPlusStatus last_accepted_status_ = PaxosPlusStatus::init;
  ballot_t last_accepted_ballot_ = 0;
};

class PaxosPlusDataCol {
 public:
  size_t count_ = 0;
  // [CURP] TODO: update last_finish_pos_
  int last_finish_pos_ = -(INT_MAX / 2);
  map<slotid_t, shared_ptr<PaxosPlusData>> logs_{};
};

struct ResponseData {
  map<pair<int, int>, vector<shared_ptr<Marshallable> > >responses_;
  int accept_count_ = 0, max_accept_count_ = 0;
  pair<int, int> append_response(const shared_ptr<Marshallable>& cmd) {
    shared_ptr<CmdData> md = dynamic_pointer_cast<CmdData>(cmd);
    pair<int, int> cmd_id = {md->client_id, md->cmd_id_in_client};
    responses_[cmd_id].push_back(cmd);
    accept_count_++;
    max_accept_count_ = max(max_accept_count_, (int)responses_[cmd_id].size());
    return {accept_count_, max_accept_count_};
  }
};

class PaxosPlusServer : public TxLogServer {
 public:
  /***************************************PLUS Begin***********************************************************/
  bool_t check_fast_path_validation(key_t key);
  value_t read(key_t key);
  slotid_t append_cmd(key_t key, shared_ptr<Marshallable>& cmd);
  /***************************************PLUS End***********************************************************/

  // ----min_active <= max_executed <= max_committed---
  slotid_t min_active_slot_ = 0; // anything before (lt) this slot is freed
  slotid_t max_executed_slot_ = 0;
  slotid_t max_committed_slot_ = 0;
  map<key_t, PaxosPlusDataCol> log_cols_{};
  int n_prepare_ = 0;
  int n_accept_ = 0;
  int n_commit_ = 0;
  bool in_applying_logs_{false};
  int test_test_test_ = -1; 

  map<pair<key_t, slotid_t>, ResponseData> response_storage_;

  MultiPaxosPlusCommo *commo_{nullptr};

  MultiPaxosPlusCommo *commo() {
    verify(commo_ != nullptr);
    return (MultiPaxosPlusCommo *) commo_;
  }

  ~PaxosPlusServer() {
    Log_info("site par %d, loc %d: prepare %d, accept %d, commit %d", partition_id_, loc_id_, n_prepare_, n_accept_, n_commit_);
  }

  void Setup();

  void OnForward(const shared_ptr<Position>& pos,
                 const shared_ptr<Marshallable>& cmd,
                 const bool_t& accepted);
  
  void OnCoordinatorAccept(const shared_ptr<Position>& pos,
                          const shared_ptr<Marshallable>& cmd,
                          bool_t* accepted,
                          const function<void()> &cb);

  void OnPrepare(const shared_ptr<Position>& pos,
                const ballot_t& ballot,
                bool_t* accepted,
                ballot_t* seen_ballot,
                int* last_accepted_status,
                shared_ptr<Marshallable>* last_accepted_cmd,
                ballot_t* last_accepted_ballot,
                const function<void()> &cb);

  void OnAccept(const shared_ptr<Position>& pos,
                const shared_ptr<Marshallable>& cmd,
                const ballot_t& ballot,
                bool_t* accepted,
                ballot_t* seen_ballot,
                const function<void()> &cb);

  void OnCommit(const shared_ptr<Position>& pos,
                const shared_ptr<Marshallable>& cmd);
};


} // namespace janus
