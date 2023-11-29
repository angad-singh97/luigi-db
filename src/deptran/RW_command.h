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
  bool same_as(SimpleRWCommand &other);
  Marshal& ToMarshal(Marshal& m) const override;
  Marshal& FromMarshal(Marshal& m) override;
  static pair<int32_t, int32_t> GetCmdID(shared_ptr<Marshallable> cmd);
  static double GetCurrentMsTime();
  static double GetCommandMsTime(shared_ptr<Marshallable> cmd);
  static double GetCommandMsTimeElaps(shared_ptr<Marshallable> cmd);
  static key_t GetKey(shared_ptr<Marshallable> cmd);
  static int64_t CombineInt32(pair<int32_t, int32_t> a) {
    return (((int64_t)a.first) << 31) | a.second;
  }
  static int64_t CombineInt32(int32_t a, int32_t b) {
    return (((int64_t)a) << 31) | b;
  }
  static pair<int32_t, int32_t> GetInt32(int64_t a) {
    return make_pair(a >> 31, a & ((1ll << 31) - 1));
  }
};

}