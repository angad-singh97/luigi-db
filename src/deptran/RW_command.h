#pragma once

#include "__dep__.h"
#include "marshallable.h"

namespace janus {

class SimpleRWCommand: public Marshallable {
 public:
  enum CmdType {NoOp=0, Read=1, Write=2};
  int32_t type_;
  key_t key_;
  int32_t value_;
  SimpleRWCommand();
  // SimpleRWCommand(const SimpleRWCommand &o);
  SimpleRWCommand(shared_ptr<Marshallable> cmd);
  std::string cmd_to_string();
  Marshal& ToMarshal(Marshal& m) const override;
  Marshal& FromMarshal(Marshal& m) override;
  static pair<int32_t, int32_t> GetCmdID(shared_ptr<Marshallable> cmd);
  static double GetCurrentMsTime();
};

}