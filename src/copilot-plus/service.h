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
};

}