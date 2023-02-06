#pragma once

#include "../__dep__.h"
#include "../constants.h"
#include "../rcc/graph.h"
#include "../rcc/graph_marshaler.h"
#include "../command.h"
#include "../procedure.h"
#include "../command_marshaler.h"
#include "../rcc_rpc.h"
#include "server.h"

namespace janus {

class CopilotPlusServiceImpl: public CopilotPlusService {
  CopilotPlusServer *svr_;
 public:
  CopilotPlusServiceImpl(TxLogServer *svr);

  void Submit(const MarshallDeputy& cmd,
              bool_t* accepted,
              slotid_t* i,
              slotid_t* j,
              rrr::DeferredReply* defer) override;
  
  void FrontRecover(const MarshallDeputy& cmd,
                    const slotid_t& i,
                    const slotid_t& j,
                    const ballot_t& ballot,
                    bool_t* accept_recover,
                    rrr::DeferredReply* defer) override;

  void FrontCommit(const MarshallDeputy& cmd,
                    const slotid_t& i,
                    const slotid_t& j,
                    const ballot_t& ballot,
                    rrr::DeferredReply* defer) override;
};

}