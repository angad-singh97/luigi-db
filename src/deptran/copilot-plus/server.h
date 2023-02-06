#pragma once

#include "../__dep__.h"
#include "../scheduler.h"

namespace janus {

struct CopilotPlusLogEle {
  enum Status_type {INIT, EXECUTED, OVER_WRITTEN, COMMITTED};
  shared_ptr<Marshallable> cmd_{nullptr};
  ballot_t ballot_ = 0;
  Status_type status_ = Status_type::INIT;
  CopilotPlusLogEle(shared_ptr<Marshallable> cmd)
    : cmd_(cmd) {}
};

struct CopilotPlusLogCol {
  key_t key_;
  bool_t committed_ = false;
  std::vector<CopilotPlusLogEle> log_col_;
  CopilotPlusLogCol(key_t key, shared_ptr<Marshallable> cmd): key_(key) {
    log_col_.push_back(CopilotPlusLogEle(cmd));
  }
};

class CopilotPlusServer : public TxLogServer {
 private:
  std::vector<CopilotPlusLogCol> logs_;
  std::map<key_t, slotid_t> lastest_slot_map_;
 public:
  CopilotPlusServer(Frame *frame);
  ~CopilotPlusServer();
  void OnSubmit(shared_ptr<Marshallable>& cmd,
                bool_t* accepted,
                slotid_t* i,
                slotid_t* j,
                const function<void()> &cb);
  void OnFrontRecover(shared_ptr<Marshallable>& cmd,
                      const slotid_t& i,
                      const slotid_t& j,
                      const ballot_t& ballot,
                      bool_t* accept_recover,
                      const function<void()> &cb);
  void OnFrontCommit(shared_ptr<Marshallable>& cmd,
                      const slotid_t& i,
                      const slotid_t& j,
                      const ballot_t& ballot,
                      const function<void()> &cb);
 private:
  void Setup();
};


}