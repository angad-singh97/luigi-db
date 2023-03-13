#pragma once

#include "../__dep__.h"
#include "../scheduler.h"
#include "RW_command.h"

namespace janus {

class BackEndData {
public:
  shared_ptr<Marshallable> cmd_{nullptr};
  slotid_t slot_id_;
  slotid_t front_i_, front_j_;
  ballot_t ballot_;
  bool_t commit_no_op_ = false;
  BackEndData():cmd_(nullptr), slot_id_(0), front_i_(0), front_j_(0), ballot_(0) {}
  BackEndData(shared_ptr<Marshallable> cmd, slotid_t slot_id, slotid_t front_i, slotid_t front_j, ballot_t ballot = 0):
    cmd_(cmd), slot_id_(slot_id), front_i_(front_i), front_j_(front_j), ballot_(ballot) {}
};

struct FrontLogEle {
  enum FrontLogStatus {INIT, EXECUTED, OVER_WRITTEN, COMMITTED};
  FrontLogStatus status_ = FrontLogStatus::INIT;
  slotid_t slot_id_;
  std::shared_ptr<BackEndData> data_;
  FrontLogEle(slotid_t slot_id, std::shared_ptr<BackEndData> data):
    slot_id_(slot_id), data_(data) {Log_info("[copilot+] FrontLogEle created with slot_id=%d", slot_id);}
  string status_str() {
    switch (status_) {
      case INIT:          return string("INIT");
      case EXECUTED:      return string("EXECUTED");
      case OVER_WRITTEN:  return string("OVER_WRITTEN");
      case COMMITTED:     return string("COMMITTED");
      default:
        verify(0);
    }
  }
  string cmd_str() {
    shared_ptr<SimpleRWCommand> cast_cmd = dynamic_pointer_cast<SimpleRWCommand>(data_->cmd_);
    return cast_cmd->cmd_to_string();
  }
};

struct FrontLogCol {
  key_t key_;
  bool_t committed_ = false;
  std::vector<FrontLogEle> log_col_;
  FrontLogCol() {}
  FrontLogCol(key_t key, slotid_t slot_id, std::shared_ptr<BackEndData> data): key_(key) {
    log_col_.push_back(FrontLogEle(slot_id, data));
  }
};

class CopilotPlusServer : public TxLogServer {
 private:
  slotid_t min_active_slot_ = 0; // anything before (lt) this slot is freed
  slotid_t max_executed_slot_ = 0;
  slotid_t max_committed_slot_ = 0;
  std::map<slotid_t, shared_ptr<BackEndData>> logs_{};
  
  slotid_t front_next_slot_ = 0;
  std::map<key_t, slotid_t> lastest_slot_map_;
  std::map<slotid_t, shared_ptr<FrontLogCol>> front_logs_;

  bool in_applying_logs_{false};

  unordered_map<uint32_t, int> executed_slots_{}; // used along with uncommitted_keys_
  int n_prepare_ = 0;
  int n_suggest_ = 0;
  int n_commit_ = 0;
 public:
  CopilotPlusServer(Frame *frame);
  ~CopilotPlusServer();

  shared_ptr<BackEndData> GetInstance(slotid_t id) {
    verify(id >= min_active_slot_);
    auto& sp_instance = logs_[id];
    if(!sp_instance) sp_instance = std::make_shared<BackEndData>();
    return sp_instance;
  }

  void OnSubmit(slotid_t slot_id,
                shared_ptr<Marshallable>& cmd,
                bool_t* accepted,
                slotid_t* i,
                slotid_t* j,
                ballot_t* ballot,
                const function<void()> &cb);
  void OnFrontRecover(slotid_t slot_id,
                      shared_ptr<Marshallable>& cmd,
                      const bool_t& commit_no_op_,
                      const slotid_t& i,
                      const slotid_t& j,
                      const ballot_t& ballot,
                      bool_t* accept_recover,
                      const function<void()> &cb);
  void OnFrontCommit(slotid_t slot_id,
                      shared_ptr<Marshallable>& cmd,
                      const bool_t& commit_no_op_,
                      const slotid_t& i,
                      const slotid_t& j,
                      const ballot_t& ballot,
                      const function<void()> &cb);
 private:
  void Setup();
  void PrintLog();
};


}