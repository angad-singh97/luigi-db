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
  bool rule_mode_on_and_is_original_path_only_command_;
  SimpleRWCommand();
  // SimpleRWCommand(const SimpleRWCommand &o);
  SimpleRWCommand(shared_ptr<Marshallable> cmd);
  std::string cmd_to_string();
  bool same_as(SimpleRWCommand &other);
  Marshal& ToMarshal(Marshal& m) const override;
  Marshal& FromMarshal(Marshal& m) override;
  bool IsRead();
  bool IsWrite();
  static pair<int32_t, int32_t> GetCmdID(shared_ptr<Marshallable> cmd);
  static uint64_t GetCombinedCmdID(shared_ptr<Marshallable> cmd);
  static double GetCurrentMsTime();
  static double GetCommandMsTime(shared_ptr<Marshallable> cmd);
  static double GetCommandMsTimeElaps(shared_ptr<Marshallable> cmd);
  static key_t GetKey(shared_ptr<Marshallable> cmd);
  static bool NeedRecordConflictInOriginalPath(shared_ptr<Marshallable> cmd);
  static uint64_t CombineInt32(pair<uint32_t, uint32_t> a) {
    return (((uint64_t)a.first) << 31) | a.second;
    // return (((uint64_t)a.first) * 1000000000) + a.second;
  }
  static uint64_t CombineInt32(uint32_t a, uint32_t b) {
    return (((uint64_t)a) << 31) | b;
    // return (((uint64_t)a) * 1000000000) + b;
  }
  static pair<uint32_t, uint32_t> GetInt32(uint64_t a) {
    return make_pair(a >> 31, a & ((1ll << 31) - 1));
    // return make_pair(a / 1000000000, a % 1000000000);
  }
  static int MaxFailure(int n) {
    return (n - 1) / 2;
  }
  static int RuleSuperMajority(int n) {
    int f = MaxFailure(n);
    return f + (f + 1) / 2 + 1;
  }
};

class KeyDistribution {
  unordered_map<key_t, int> key_count_;
  vector<pair<int, key_t>> sort_vec_;
 public:
  void Insert(key_t key);
  void Print();
};

}