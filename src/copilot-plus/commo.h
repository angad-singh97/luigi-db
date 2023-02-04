#pragma once

#include "../__dep__.h"
#include "../communicator.h"

namespace janus {

class CopilotPlusCommo: public Communicator {
 public:
  CopilotPlusCommo() = delete;
  CopilotPlusCommo(PollMgr *poll);
};


}