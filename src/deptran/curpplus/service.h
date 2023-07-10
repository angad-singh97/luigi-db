#pragma once

#include "__dep__.h"
#include "constants.h"
#include "../rcc/graph.h"
#include "../rcc/graph_marshaler.h"
#include "../command.h"
#include "deptran/procedure.h"
#include "../command_marshaler.h"
#include "../rcc_rpc.h"
#include <chrono>

class SimpleCommand;
namespace janus {

class TxLogServer;
class CurpPlusServer;
class CurpPlusServiceImpl : public CurpPlusService {
 public:
  CurpPlusServer* sched_;
  CurpPlusServiceImpl(TxLogServer* sched);

  // void Dispatch(const int32_t& client_id,
  //               const int32_t& cmd_id_in_client,
  //               const MarshallDeputy& cmd,
  //               bool_t* accepted,
  //               MarshallDeputy* pos,
  //               int32_t* result,
  //               siteid_t* coo_id,
  //               rrr::DeferredReply* defer) override;

  void PoorDispatch(const int32_t& client_id,
                    const int32_t& cmd_id_in_client,
                    const MarshallDeputy& cmd,
                    bool_t* accepted,
                    pos_t* pos0,
                    pos_t* pos1,
                    int32_t* result,
                    siteid_t* coo_id,
                    rrr::DeferredReply* defer) override;

  void WaitCommit(const int32_t& client_id,
                  const int32_t& cmd_id_in_client,
                  bool_t* committed,
                  rrr::DeferredReply* defer) override;

  void Forward(const MarshallDeputy& pos,
                const MarshallDeputy& cmd,
                const bool_t& accepted,
                rrr::DeferredReply* defer) override;
  
  // void PoorForward(const pos_t& pos0,
  //                   const pos_t& pos1,
  //                   const MarshallDeputy& cmd,
  //                   const bool_t& accepted,
  //                   rrr::DeferredReply* defer) override;

  void CoordinatorAccept(const MarshallDeputy& pos,
                          const MarshallDeputy& cmd,
                          bool_t* accepted, rrr::DeferredReply* defer) override;

  // void PoorCoordinatorAccept(const pos_t& pos0,
  //                             const pos_t& pos1,
  //                             const MarshallDeputy& cmd,
  //                             bool_t* accepted, rrr::DeferredReply* defer) override;

  void Prepare(const MarshallDeputy& pos,
              const ballot_t& ballot,
              bool_t* accepted,
              ballot_t* seen_ballot,
              rrr::i32* last_accepted_status,
              MarshallDeputy* last_accepted_cmd,
              ballot_t* last_accepted_ballot,
              rrr::DeferredReply* defer) override;
  
  // void PoorPrepare(const pos_t& pos0,
  //                   const pos_t& pos1,
  //                   const ballot_t& ballot,
  //                   bool_t* accepted,
  //                   ballot_t* seen_ballot,
  //                   rrr::i32* last_accepted_status,
  //                   MarshallDeputy* last_accepted_cmd,
  //                   ballot_t* last_accepted_ballot,
  //                   rrr::DeferredReply* defer) override;

  void Accept(const MarshallDeputy& pos,
              const MarshallDeputy& md_cmd,
              const ballot_t& ballot,
              bool_t* accepted,
              ballot_t* seen_ballot,
              rrr::DeferredReply* defer) override;
  
  // void PoorAccept(const pos_t& pos0,
  //                 const pos_t& pos1,
  //                 const MarshallDeputy& md_cmd,
  //                 const ballot_t& ballot,
  //                 bool_t* accepted,
  //                 ballot_t* seen_ballot,
  //                 rrr::DeferredReply* defer) override;
  
  void Commit(const MarshallDeputy& pos,
              const MarshallDeputy& md_cmd,
              rrr::DeferredReply* defer) override;

  // void PoorCommit(const pos_t& pos0,
  //                 const pos_t& pos1,
  //                 const MarshallDeputy& md_cmd,
  //                 rrr::DeferredReply* defer) override;
};

} // namespace janus
