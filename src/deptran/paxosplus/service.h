#pragma once

#include "__dep__.h"
#include "constants.h"
#include "../rcc/graph.h"
#include "../rcc/graph_marshaler.h"
#include "../command.h"
#include "deptran/procedure.h"
#include "../command_marshaler.h"
#include "../rcc_rpc.h"
#include "../curp_service.h"
#include <chrono>

class SimpleCommand;
namespace janus {

class TxLogServer;
class PaxosPlusServer;
class MultiPaxosPlusServiceImpl : public MultiPaxosPlusService, public CurpServiceImpl {
 public:
  // PaxosPlusServer* sched_;
  MultiPaxosPlusServiceImpl(TxLogServer* sched);

  PaxosPlusServer* sched() {
    verify(sched_);
    return (PaxosPlusServer*)sched_;
  }

  int __reg_to__(rrr::Server* svr) override {
    MultiPaxosPlusService::__reg_to__(svr);
    CurpServiceImpl::__reg_to__(svr);
    return 0;
  }

  void Forward(const MarshallDeputy& cmd,
               const uint64_t& dep_id,
               uint64_t* coro_id,
               rrr::DeferredReply* defer) override;

  void Prepare(const uint64_t& slot,
               const ballot_t& ballot,
               ballot_t* max_ballot,
               uint64_t* coro_id,
               rrr::DeferredReply* defer) override;

  void Accept(const uint64_t& slot,
              const uint64_t& time,
              const ballot_t& ballot,
              const MarshallDeputy& cmd,
              const uint64_t& commit_finish,
              ballot_t* max_ballot,
              uint64_t* coro_id,
              bool_t* finish_accept,
              uint64_t* finish_ver,
              rrr::DeferredReply* defer) override;

  void Decide(const uint64_t& slot,
              const ballot_t& ballot,
              const MarshallDeputy& cmd,
              rrr::DeferredReply* defer) override;
};

} // namespace janus
