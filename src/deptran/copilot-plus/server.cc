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

void CopilotPlusServer::OnSubmit(shared_ptr<Marshallable>& cmd,
                                  bool_t* accepted,
                                  slotid_t* i,
                                  slotid_t* j,
                                  ballot_t* ballot,
                                  const function<void()> &cb) {
  Log_info("[copilot+] server enter OnSubmit, this->loc_id_=%d", this->loc_id_);
  std::lock_guard<std::recursive_mutex> lock(mtx_);
  key_t key = dynamic_pointer_cast<SimpleRWCommand>(cmd)->key_;
  auto lastest_slot = lastest_slot_map_.find(key);
  if (lastest_slot == lastest_slot_map_.end()) {
    Log_info("[copilot+] On Commit Branch 1 key didn't appear before");
    // uncommited key column didn't appear before
    logs_.push_back(CopilotPlusLogCol(key, cmd));
    *accepted = true;
    *i = (slotid_t)(logs_.size() - 1);
    *j = (slotid_t)(logs_.back().log_col_.size() - 1);
    *ballot = 0;
    // TODO: run command
    logs_[*i].log_col_[*j].status_ = CopilotPlusLogEle::Status_type::EXECUTED;
    Log_info("[copilot+] %d %d %d %d %d %d", (slotid_t)(logs_.size() - 1), (slotid_t)(logs_.back().log_col_.size() - 1), 0, *i, *j, *ballot);
  } else {
    Log_info("[copilot+] On Commit Branch 2 key appear before");
    // uncommited key column appear before
    slotid_t slot = lastest_slot->second;
    if (logs_[slot].log_col_.back().status_ == CopilotPlusLogEle::Status_type::COMMITTED) {
       logs_[slot].log_col_.push_back(CopilotPlusLogEle(cmd));
      *accepted = true;
      *i = (slotid_t)(logs_.size() - 1);
      *j = (slotid_t)(logs_.back().log_col_.size() - 1);
      *ballot = 0;
      // TODO: run command
      logs_[*i].log_col_[*j].status_ = CopilotPlusLogEle::Status_type::EXECUTED;
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

void CopilotPlusServer::OnFrontRecover(shared_ptr<Marshallable>& cmd,
                                        const slotid_t& i,
                                        const slotid_t& j,
                                        const ballot_t& ballot,
                                        bool_t* up_to_date,
                                        const function<void()> &cb) {
  Log_info("[copilot+] server enter OnFrontRecover, this->loc_id_=%d", this->loc_id_);
  std::lock_guard<std::recursive_mutex> lock(mtx_);
  while (logs_[i].log_col_.size() > j + 1)
    logs_[i].log_col_.pop_back();
  // TODO: recover a whole cmd vector
  CopilotPlusLogEle *log = &logs_[i].log_col_[j];
  log->cmd_ = cmd;
  log->ballot_ = ballot;
  log->status_ = CopilotPlusLogEle::Status_type::OVER_WRITTEN;
  *up_to_date = true;
  cb();
}

void CopilotPlusServer::OnFrontCommit(shared_ptr<Marshallable>& cmd,
                    const slotid_t& i,
                    const slotid_t& j,
                    const ballot_t& ballot,
                    const function<void()> &cb) {
  Log_info("[copilot+] server enter OnFrontCommit, this->loc_id_=%d, i=%d, j=%d, ballot=%d", this->loc_id_, i, j, ballot);
  PrintLog();
  std::lock_guard<std::recursive_mutex> lock(mtx_);
  while (logs_[i].log_col_.size() > j + 1)
    logs_[i].log_col_.pop_back();
  // TODO: recover a whole cmd vector
  CopilotPlusLogEle *log = &logs_[i].log_col_[j];
  log->cmd_ = cmd;
  log->ballot_ = ballot;
  log->status_ = CopilotPlusLogEle::Status_type::COMMITTED;
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
    string tmp("[key_=%d][committed=%d] ", logs_[i].key_, logs_[i].committed_);
    for (int j = 0; j < logs_[i].log_col_.size(); j++) {
      tmp += string("(%d) ", j) + logs_[i].log_col_[j].status_str() + " " + logs_[i].log_col_[j].status_str() + " ";
    }
    Log_info(tmp.c_str());
  }
  Log_info("/************** Print Log End ******************/");
}

};