#include "../__dep__.h"
#include "server.h"
#include "frame.h"
//#include "tx.h"
#include "../classic/tpc_command.h"

namespace janus {


CopilotPlusServer::CopilotPlusServer(Frame *frame) {
  frame_ = frame;
  Log_info("Created this=%p, this->loc_id_=%d, this->commo_==%p", (void*)this, this->loc_id_, (void*)this->commo_);
  //Setup();
} 

CopilotPlusServer::~CopilotPlusServer() {

}

void CopilotPlusServer::OnSubmit(slotid_t slot_id,
                                  shared_ptr<Marshallable>& cmd,
                                  bool_t* accepted,
                                  slotid_t* i,
                                  slotid_t* j,
                                  ballot_t* ballot,
                                  const function<void()> &cb) {
  Log_info("[copilot+] server enter OnSubmit, this->loc_id_=%d", this->loc_id_);
  std::lock_guard<std::recursive_mutex> lock(mtx_);

  shared_ptr<SimpleRWCommand> parsed_cmd_ = make_shared<SimpleRWCommand>(cmd);
  key_t key = dynamic_pointer_cast<SimpleRWCommand>(parsed_cmd_)->key_;
  auto lastest_slot = lastest_slot_map_.find(key);
  if (lastest_slot == lastest_slot_map_.end()) {
    Log_info("[copilot+] On Commit Branch 1 key didn't appear before");
    *accepted = true;
    *i = front_next_slot_;
    *j = 0;
    *ballot = 0;
    logs_[slot_id] = make_shared<BackEndData>(cmd, slot_id, *i, *j, *ballot);
    front_logs_[*i] = make_shared<FrontLogCol>(key, slot_id, logs_[slot_id]);
    
    // TODO: run command
    front_logs_[*i]->log_col_[*j].status_ = FrontLogEle::FrontLogStatus::EXECUTED;
    lastest_slot_map_[key] = front_next_slot_;
    front_next_slot_++;
  } else {
    Log_info("[copilot+] On Commit Branch 2 key appear before");
    slotid_t slot = lastest_slot->second;
    if (front_logs_[slot]->log_col_.back().status_ == FrontLogEle::FrontLogStatus::COMMITTED) {
      *accepted = true;
      *i = slot;
      *j = (slotid_t)(front_logs_[slot]->log_col_.size() - 1);
      *ballot = 0;
      logs_[slot_id] = make_shared<BackEndData>(cmd, slot_id, *i, *j, *ballot);
      front_logs_[slot]->log_col_.push_back(FrontLogEle(slot_id, logs_[slot_id]));

      // TODO: run command
      front_logs_[*i]->log_col_[*j].status_ = FrontLogEle::FrontLogStatus::EXECUTED;
    } else {
      *accepted = false;
      *i = -1;
      *j = -1;
      *ballot = 0;
    }
  }
  Log_info("[copilot+] exit OnSubmit with i=%d j=%d ballot=%d accepted=%d", *i, *j, *ballot, *accepted);
  cb();
}

void CopilotPlusServer::OnFrontRecover(slotid_t slot_id,
                                        shared_ptr<Marshallable>& cmd,
                                        const bool_t& commit_no_op_,
                                        const slotid_t& i,
                                        const slotid_t& j,
                                        const ballot_t& ballot,
                                        bool_t* up_to_date,
                                        const function<void()> &cb) {
  Log_info("[copilot+] server enter OnFrontRecover, this->loc_id_=%d", this->loc_id_);
  std::lock_guard<std::recursive_mutex> lock(mtx_);
  // TODO: abort recover
  shared_ptr<BackEndData> log = GetInstance(slot_id);
  log->cmd_ = cmd;
  log->ballot_ = ballot;
  if (commit_no_op_) log->commit_no_op_ = commit_no_op_;
  front_logs_[log->front_i_]->log_col_[log->front_j_].status_ = FrontLogEle::FrontLogStatus::OVER_WRITTEN;
  *up_to_date = true;
  cb();
}

void CopilotPlusServer::OnFrontCommit(slotid_t slot_id,
                                      shared_ptr<Marshallable>& cmd,
                                      const bool_t& commit_no_op_,
                                      const slotid_t& i,
                                      const slotid_t& j,
                                      const ballot_t& ballot,
                                      const function<void()> &cb) {
  Log_info("[copilot+] server enter OnFrontCommit, this->loc_id_=%d, i=%d, j=%d, ballot=%d", this->loc_id_, i, j, ballot);
  //PrintLog();
  std::lock_guard<std::recursive_mutex> lock(mtx_);
  // TODO: abort recover
  shared_ptr<BackEndData> log = GetInstance(slot_id);
  log->cmd_ = cmd;
  log->ballot_ = ballot;
  if (commit_no_op_) log->commit_no_op_ = commit_no_op_;
  front_logs_[log->front_i_]->log_col_[log->front_j_].status_ = FrontLogEle::FrontLogStatus::OVER_WRITTEN;

  shared_ptr<BackEndData> instance = GetInstance(slot_id);
  if (slot_id > max_committed_slot_) {
    max_committed_slot_ = slot_id;
  }
  verify(slot_id > max_executed_slot_);
  // This prevents the log entry from being applied twice
  if (in_applying_logs_) {
    return;
  }
  in_applying_logs_ = true;

  for (slotid_t id = max_executed_slot_ + 1; id <= max_committed_slot_; id++) {
    auto next_instance = GetInstance(id);
    if (next_instance->cmd_) {
      if (executed_slots_[id]!=1){
        app_next_(*next_instance->cmd_);
        executed_slots_.erase(id);
      }
        
      Log_debug("frontend par:%d loc:%d executed slot %lx now", partition_id_, loc_id_, id);
      max_executed_slot_++;
      n_commit_++;
    } else {
      break;
    }
  }

  cb();
}

void CopilotPlusServer::Setup() {
  Log_info("Setup this=%p, this->loc_id_=%d, this->commo_==%p", (void*)this, this->loc_id_, (void*)this->commo_);
  //while (this->commo_ == nullptr) {}
  //static_cast<CopilotPlusCommo*>(this->commo_)->setServer(static_cast<CopilotPlusServer*>(this->rep_sched_));
}

void CopilotPlusServer::PrintLog() {
  std::lock_guard<std::recursive_mutex> lock(mtx_);
  Log_info("/************** Print Log Begin ******************/");
  for (int i = 0; i < logs_.size(); i++) {
    string tmp("svr " + to_string(loc_id_) + " slice " + to_string(i) + " [key_=" + to_string(front_logs_[i]->key_) + "][committed=" + to_string(front_logs_[i]->committed_) + "] ");
    for (int j = 0; j < front_logs_[i]->log_col_.size(); j++) {
      tmp += "(" + to_string(j) + ") " + front_logs_[i]->log_col_[j].status_str() + " " + front_logs_[i]->log_col_[j].cmd_str() + " ";
    }
    Log_info(tmp.c_str());
  }
  Log_info("/************** Print Log End ******************/");
}

};