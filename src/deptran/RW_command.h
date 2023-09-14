#pragma once

#include "__dep__.h"
#include "marshallable.h"

namespace janus {

class SimpleRWCommand: public Marshallable {
 public:
  int32_t type_;
  key_t key_;
  int32_t value_;
  pair<int32_t, int32_t> cmd_id_;
  SimpleRWCommand();
  // SimpleRWCommand(const SimpleRWCommand &o);
  SimpleRWCommand(shared_ptr<Marshallable> cmd);
  std::string cmd_to_string();
  Marshal& ToMarshal(Marshal& m) const override;
  Marshal& FromMarshal(Marshal& m) override;
  static pair<int32_t, int32_t> GetCmdID(shared_ptr<Marshallable> cmd);
  static double GetCurrentMsTime();
  static key_t GetKey(shared_ptr<Marshallable> cmd);
};

}