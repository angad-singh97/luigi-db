#pragma once

#include "../__dep__.h"
#include "../constants.h"
#include "../rcc/graph.h"
#include "../rcc/graph_marshaler.h"
#include "../command.h"
#include "../procedure.h"
#include "../command_marshaler.h"
#include "../rcc_rpc.h"

namespace janus {

class TxLogServer;
class CopilotPlusServer;

class CopilotPlusServiceImpl : public CopilotPlusService {
  CopilotPlusServer* sched_;
 public:
  CopilotPlusServiceImpl(TxLogServer *sched);

  void Forward(const MarshallDeputy& cmd,
               rrr::DeferredReply* defer) override;

  void Prepare(const uint8_t& is_pilot,
               const uint64_t& slot,
               const ballot_t& ballot,
               const struct DepId& dep_id,
               MarshallDeputy* ret_cmd,
               ballot_t* max_ballot,
               uint64_t* dep,
               status_t* status,
               rrr::DeferredReply* defer) override;

  void FastAccept(const uint8_t& is_pilot,
                  const uint64_t& slot,
                  const ballot_t& ballot,
                  const uint64_t& dep,
                  const MarshallDeputy& cmd,
                  const struct DepId& dep_id,
                  ballot_t* max_ballot,
                  uint64_t* ret_dep,
                  rrr::DeferredReply* defer) override;

  void Accept(const uint8_t& is_pilot,
              const uint64_t& slot,
              const ballot_t& ballot,
              const uint64_t& dep,
              const MarshallDeputy& cmd,
              const struct DepId& dep_id,
              ballot_t* max_ballot,
              rrr::DeferredReply* defer) override;

  void Commit(const uint8_t& is_pilot,
              const uint64_t& slot,
              const uint64_t& dep,
              const MarshallDeputy& cmd,
              rrr::DeferredReply* defer) override;
  
  // below are about CURP

  void CurpPoorDispatch(const int32_t& client_id,
                    const int32_t& cmd_id_in_client,
                    const MarshallDeputy& cmd,
                    bool_t* accepted,
                    pos_t* pos0,
                    pos_t* pos1,
                    int32_t* result,
                    siteid_t* coo_id,
                    rrr::DeferredReply* defer) override;

  void CurpWaitCommit(const int32_t& client_id,
                  const int32_t& cmd_id_in_client,
                  bool_t* committed,
                  rrr::DeferredReply* defer) override;

  void CurpForward(const MarshallDeputy& pos,
                const MarshallDeputy& cmd,
                const bool_t& accepted,
                rrr::DeferredReply* defer) override;

  void CurpCoordinatorAccept(const MarshallDeputy& pos,
                          const MarshallDeputy& cmd,
                          bool_t* accepted, rrr::DeferredReply* defer) override;

  void CurpPrepare(const MarshallDeputy& pos,
              const ballot_t& ballot,
              bool_t* accepted,
              ballot_t* seen_ballot,
              rrr::i32* last_accepted_status,
              MarshallDeputy* last_accepted_cmd,
              ballot_t* last_accepted_ballot,
              rrr::DeferredReply* defer) override;

  void CurpAccept(const MarshallDeputy& pos,
              const MarshallDeputy& md_cmd,
              const ballot_t& ballot,
              bool_t* accepted,
              ballot_t* seen_ballot,
              rrr::DeferredReply* defer) override;
  
  void CurpCommit(const MarshallDeputy& pos,
              const MarshallDeputy& md_cmd,
              rrr::DeferredReply* defer) override;

};

} // namespace janus