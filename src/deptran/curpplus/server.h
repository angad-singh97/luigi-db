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

struct CurpPlusData {
  enum CurpPlusStatus {
    init = 0,
    fastAccepted = 1,
    prepared = 2,
    accepted = 3,
    committed = 4
  };
  CurpPlusStatus status_ = CurpPlusStatus::init;
  ballot_t max_ballot_seen_ = 0;
  ballot_t max_ballot_accepted_ = 0;
  shared_ptr<Marshallable> fast_accepted_cmd_{nullptr};
  shared_ptr<Marshallable> accepted_cmd_{nullptr};
  shared_ptr<Marshallable> committed_cmd_{nullptr};
  
  shared_ptr<Marshallable> last_accepted_{nullptr};
  CurpPlusStatus last_accepted_status_ = CurpPlusStatus::init;
  ballot_t last_accepted_ballot_ = 0;
};

class CurpPlusDataCol {
 public:
  size_t count_ = 0;
  // [CURP] TODO: update last_finish_pos_
  int last_finish_pos_ = -(INT_MAX / 2);
  map<slotid_t, shared_ptr<CurpPlusData>> logs_{};
};

struct ResponseData {
  map<pair<int, int>, vector<shared_ptr<Marshallable> > >responses_;
  int accept_count_ = 0, max_accept_count_ = 0;
  pair<int, int> append_response(const shared_ptr<Marshallable>& cmd) {
    shared_ptr<CmdData> md = dynamic_pointer_cast<CmdData>(cmd);
    pair<int, int> cmd_id = {md->client_id_, md->cmd_id_in_client_};
    responses_[cmd_id].push_back(cmd);
    accept_count_++;
    max_accept_count_ = max(max_accept_count_, (int)responses_[cmd_id].size());
    return {accept_count_, max_accept_count_};
  }
};

class CurpPlusServer : public TxLogServer {
 public:
  /***************************************PLUS Begin***********************************************************/
  bool_t check_fast_path_validation(key_t key);
  value_t read(key_t key);
  slotid_t append_cmd(key_t key, const shared_ptr<Marshallable>& cmd);
  /***************************************PLUS End***********************************************************/

  // ----min_active <= max_executed <= max_committed---
  slotid_t min_active_slot_ = 0; // anything before (lt) this slot is freed
  slotid_t max_executed_slot_ = 0;
  slotid_t max_committed_slot_ = 0;
  map<key_t, CurpPlusDataCol> log_cols_{};
  int n_prepare_ = 0;
  int n_accept_ = 0;
  int n_commit_ = 0;
  bool in_applying_logs_{false};
  int test_test_test_ = -1; 

  map<pair<key_t, slotid_t>, ResponseData> response_storage_;

  CurpPlusCommo *commo() {
    verify(commo_ != nullptr);
    return (CurpPlusCommo *) commo_;
  }

  CurpPlusServer() {
    Log_info("CurpPlusServer Created, site par %d, loc %d", partition_id_, loc_id_);
  }

  ~CurpPlusServer() {
    Log_info("site par %d, loc %d: prepare %d, accept %d, commit %d", partition_id_, loc_id_, n_prepare_, n_accept_, n_commit_);
  }

  void Setup();

  void OnDispatch(const int32_t& client_id,
                  const int32_t& cmd_id_in_client,
                  const shared_ptr<Marshallable>& cmd,
                  bool_t* accepted,
                  MarshallDeputy* pos_deputy,
                  value_t* result,
                  siteid_t* coo_id,
                  const function<void()> &cb);
  
  void OnWaitCommit(const int32_t& client_id,
                    const int32_t& cmd_id_in_client,
                    bool_t* committed,
                    const function<void()> &cb);

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
