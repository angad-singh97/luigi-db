#pragma once

#include "../__dep__.h"
#include "../scheduler.h"

namespace janus {

struct FastData {
  shared_ptr<Marshallable> cmd{nullptr};
};

struct FastLogInfo {
  std::vector<FastData> logs;
};

class CopilotPlusServer : public TxLogServer {
 private:
  std::vector<FastLogInfo> fast_log_;
 public:
  CopilotPlusServer(Frame *frame);
  ~CopilotPlusServer();
  void OnSubmit(const MarshallDeputy& cmd,
                slotid_t* i,
                slotid_t* j,
                ballot_t* ballot,
                const function<void()> &cb);
  void OnFrontRecover(const MarshallDeputy& cmd,
                      const slotid_t& i,
                      const slotid_t& j,
                      const ballot_t& ballot,
                      bool_t* accept_recover,
                      const function<void()> &cb);
  void OnFrontCommit(const MarshallDeputy& cmd,
                      const slotid_t& i,
                      const slotid_t& j,
                      const ballot_t& ballot,
                      const function<void()> &cb);
 private:
  void Setup();
};


}
