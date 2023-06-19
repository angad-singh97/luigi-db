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
class PaxosPlusServer;
class MultiPaxosPlusServiceImpl : public MultiPaxosPlusService {
 public:
  PaxosPlusServer* sched_;
  MultiPaxosPlusServiceImpl(TxLogServer* sched);

  void Forward(const MarshallDeputy& pos,
                const MarshallDeputy& cmd,
                const bool_t& accepted,
                rrr::DeferredReply* defer) override;

  void CoordinatorAccept(const MarshallDeputy& pos,
                          const MarshallDeputy& cmd,
                          bool_t* accepted, rrr::DeferredReply* defer) override;

  void Prepare(const MarshallDeputy& pos,
              const ballot_t& ballot,
              bool_t* accepted,
              ballot_t* seen_ballot,
              rrr::i32* last_accepted_status,
              MarshallDeputy* last_accepted_cmd,
              ballot_t* last_accepted_ballot,
              rrr::DeferredReply* defer) override;
  
  void Accept(const MarshallDeputy& pos,
              const MarshallDeputy& md_cmd,
              const ballot_t& ballot,
              bool_t* accepted,
              ballot_t* seen_ballot,
              rrr::DeferredReply* defer) override;
  
  void Commit(const MarshallDeputy& pos,
              const MarshallDeputy& md_cmd,
              rrr::DeferredReply* defer) override;

};

} // namespace janus
