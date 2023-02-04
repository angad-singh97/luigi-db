#pragma once

#include "../__dep__.h"
#include "../scheduler.h"

namespace janus {


class CopilotPlusServer : public TxLogServer {
 private:
 public:
  CopilotPlusServer(Frame *frame);
  ~CopilotPlusServer();
 private:
  void Setup();
};


}