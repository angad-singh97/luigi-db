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



class CurpPlusServer : public TxLogServer {
 public:
  // /***************************************PLUS Begin***********************************************************/
  // bool_t check_fast_path_validation(key_t key);
  // value_t read(key_t key);
  // slotid_t append_cmd(key_t key, const shared_ptr<Marshallable>& cmd);
  // /***************************************PLUS End***********************************************************/

  // // ----min_active <= max_executed <= max_committed---
  // slotid_t global_min_active_slot_ = 0; // anything before (lt) this slot is freed
  // slotid_t global_max_executed_slot_ = 0;
  // slotid_t global_max_committed_slot_ = 0;
  // map<key_t, shared_ptr<CurpPlusDataCol> > curp_log_cols_{};
  // int n_prepare_ = 0;
  // int n_accept_ = 0;
  // int n_commit_ = 0;
  // bool in_applying_logs_{false};

  // // global id related
  // int curp_global_id_hinter_ = 1;

  // map<pair<pos_t, pos_t>, ResponseData> curp_response_storage_;

  CurpPlusCommo *commo() {
    verify(commo_ != nullptr);
    return (CurpPlusCommo *) commo_;
  }

  CurpPlusServer() {
    Log_info("CurpPlusServer Created, site par %d, loc %d", partition_id_, loc_id_);
  }

  ~CurpPlusServer() {
    // Log_info("site par %d, loc %d: prepare %d, accept %d, commit %d", partition_id_, loc_id_, n_prepare_, n_accept_, n_commit_);
  }

  void Setup();

  // void OnDispatch(const int32_t& client_id,
  //                 const int32_t& cmd_id_in_client,
  //                 const shared_ptr<Marshallable>& cmd,
  //                 bool_t* accepted,
  //                 MarshallDeputy* pos_deputy,
  //                 value_t* result,
  //                 siteid_t* coo_id,
  //                 const function<void()> &cb);
  
  // void OnPoorDispatch(const int32_t& client_id,
  //                     const int32_t& cmd_id_in_client,
  //                     const shared_ptr<Marshallable>& cmd,
  //                     bool_t* accepted,
  //                     pos_t* pos0,
  //                     pos_t* pos1,
  //                     value_t* result,
  //                     siteid_t* coo_id,
  //                     const function<void()> &cb);

  // void OnWaitCommit(const int32_t& client_id,
  //                   const int32_t& cmd_id_in_client,
  //                   bool_t* committed,
  //                   const function<void()> &cb);

  // void OnForward(const shared_ptr<Position>& pos,
  //                const shared_ptr<Marshallable>& cmd,
  //                const bool_t& accepted);
  
  // void OnCoordinatorAccept(const shared_ptr<Position>& pos,
  //                         const shared_ptr<Marshallable>& cmd,
  //                         bool_t* accepted,
  //                         const function<void()> &cb);

  // void OnPrepare(const shared_ptr<Position>& pos,
  //               const ballot_t& ballot,
  //               bool_t* accepted,
  //               ballot_t* seen_ballot,
  //               int* last_accepted_status,
  //               shared_ptr<Marshallable>* last_accepted_cmd,
  //               ballot_t* last_accepted_ballot,
  //               const function<void()> &cb);

  // void OnAccept(const shared_ptr<Position>& pos,
  //               const shared_ptr<Marshallable>& cmd,
  //               const ballot_t& ballot,
  //               bool_t* accepted,
  //               ballot_t* seen_ballot,
  //               const function<void()> &cb);

  // void OnCommit(const shared_ptr<Position>& pos,
  //               const shared_ptr<Marshallable>& cmd);

  // void Commit(shared_ptr<Position> pos,
  //             shared_ptr<Marshallable> cmd);

  // slotid_t ApplyForNewGlobalID();

  // slotid_t OriginalProtocolApplyForNewGlobalID(key_t key);

  // UniqueCmdID GetUniqueCmdID(shared_ptr<Marshallable> cmd);

};

} // namespace janus
