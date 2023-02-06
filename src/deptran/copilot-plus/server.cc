#include "../__dep__.h"
#include "server.h"
#include "frame.h"
#include "tx.h"

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
                                  const function<void()> &cb) {
  Log_info("enter OnSubmit");
  std::lock_guard<std::recursive_mutex> lock(mtx_);
  KeyValueCommand& md = dynamic_cast<KeyValueCommand&>(*cmd);
  key_t key = md.key_;
  auto lastest_slot = lastest_slot_map_.find(key);
  if (lastest_slot == lastest_slot_map_.end()) {
    Log_info("On Commit Branch 1");
    // uncommited key column didn't appear before
    logs_.push_back(CopilotPlusLogCol(key, cmd));
    *accepted = true;
    *i = (slotid_t)(logs_.size() - 1);
    *j = (slotid_t)(logs_.back().log_col_.size() - 1);
    // TODO: run command
    logs_[*i].log_col_[*j].status_ = CopilotPlusLogEle::Status_type::EXECUTED;
  } else {
    Log_info("On Commit Branch 2");
    // uncommited key column appear before
    slotid_t slot = lastest_slot->second;
    if (logs_[slot].log_col_.back().status_ == CopilotPlusLogEle::Status_type::COMMITTED) {
       logs_[slot].log_col_.push_back(CopilotPlusLogEle(cmd));
      *accepted = true;
      *i = (slotid_t)(logs_.size() - 1);
      *j = (slotid_t)(logs_.back().log_col_.size() - 1);
      // TODO: run command
      logs_[*i].log_col_[*j].status_ = CopilotPlusLogEle::Status_type::EXECUTED;
    } else {
      *accepted = false;
      *i = -1;
      *j = -1;
    }
  }
  Log_info("exit OnSubmit");
}

void CopilotPlusServer::OnFrontRecover(shared_ptr<Marshallable>& cmd,
                                        const slotid_t& i,
                                        const slotid_t& j,
                                        const ballot_t& ballot,
                                        bool_t* up_to_date,
                                        const function<void()> &cb) {
  std::lock_guard<std::recursive_mutex> lock(mtx_);
  while (logs_[i].log_col_.size() > j + 1)
    logs_[i].log_col_.pop_back();
  // TODO: recover a whole cmd vector
  CopilotPlusLogEle *log = &logs_[i].log_col_[j];
  log->cmd_ = cmd;
  log->ballot_ = ballot;
  log->status_ = CopilotPlusLogEle::Status_type::OVER_WRITTEN;
  *up_to_date = true;
}

void CopilotPlusServer::OnFrontCommit(shared_ptr<Marshallable>& cmd,
                    const slotid_t& i,
                    const slotid_t& j,
                    const ballot_t& ballot,
                    const function<void()> &cb) {
  std::lock_guard<std::recursive_mutex> lock(mtx_);
  while (logs_[i].log_col_.size() > j + 1)
    logs_[i].log_col_.pop_back();
  // TODO: recover a whole cmd vector
  CopilotPlusLogEle *log = &logs_[i].log_col_[j];
  log->cmd_ = cmd;
  log->ballot_ = ballot;
  log->status_ = CopilotPlusLogEle::Status_type::COMMITTED;
}

void CopilotPlusServer::Setup() {
  Log_info("Setup this=%p, this->loc_id_=%d, this->commo_==%p", (void*)this, this->loc_id_, (void*)this->commo_);
  //while (this->commo_ == nullptr) {}
  //static_cast<CopilotPlusCommo*>(this->commo_)->setServer(static_cast<CopilotPlusServer*>(this->rep_sched_));
}


};