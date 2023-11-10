#pragma once

#include "../__dep__.h"
#include "../constants.h"
#include "../rcc/graph.h"
#include "../rcc/graph_marshaler.h"
#include "../command.h"
#include "../procedure.h"
#include "../command_marshaler.h"
#include "../rcc_rpc.h"
#include "../curp_service.h"

namespace janus {

class TxLogServer;
class CopilotPlusServer;

class CopilotPlusServiceImpl : public CopilotPlusService, public CurpServiceImpl {
  // CopilotPlusServer* sched_;
 public:
  CopilotPlusServiceImpl(TxLogServer *sched);

  CopilotPlusServer* sched() {
    verify(sched_);
    return (CopilotPlusServer*)sched_;
  }

  int __reg_to__(rrr::Server* svr) override {
    CopilotPlusService::__reg_to__(svr);
    CurpServiceImpl::__reg_to__(svr);
    return 0;
  }

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
                  const uint64_t& commit_finish,
                  ballot_t* max_ballot,
                  uint64_t* ret_dep,
                  bool_t* finish_accept,
                  uint64_t* finish_ver,
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
};

} // namespace janus