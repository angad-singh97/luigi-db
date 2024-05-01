#pragma once

#include "__dep__.h"
#include "constants.h"
#include "../scheduler.h"
#include "../mongodb_kv_table_handler.h"
#include "../mongodb_connection_thread_pool.h"
#include "../communicator.h"

namespace janus {

class MongodbServer : public TxLogServer {
  
#ifdef AWS
  const int mongodb_connection_ = 4000; // [JetPack] seems mongodb community default maximum connextion between 4000 and 4500 in rs
#endif
#ifndef AWS
  const int mongodb_connection_ = 80; // [JetPack] seems mongodb community default maximum connextion between 95 * 5 and 100 * 5 at local
#endif
  // const int mongodb_connection_ = 1;
  shared_ptr<MongodbConnectionThreadPool> mongodb_;
  std::thread execution_thread;

  static void ExecutionHandler(MongodbServer* svr, shared_ptr<MongodbConnectionThreadPool>& db) {
    while (true) { // This is not hot loop
      shared_ptr<Marshallable> cmd = db->MongodbFinishedPop();
      if (cmd == nullptr) {
        Log_info("Exit ExecutionHandler for nullptr");
        break;
      }
      svr->RuleWitnessGC(cmd);
#ifdef MONGODB_DEBUG
      Log_info("%.2f After RuleWitnessGC <%d, %d>", SimpleRWCommand::GetMsTimeElaps(), SimpleRWCommand::GetCmdID(cmd).first, SimpleRWCommand::GetCmdID(cmd).second);
#endif
      svr->app_next_(*cmd);
#ifdef MONGODB_DEBUG
      Log_info("%.2f After app_next_ <%d, %d>", SimpleRWCommand::GetMsTimeElaps(), SimpleRWCommand::GetCmdID(cmd).first, SimpleRWCommand::GetCmdID(cmd).second);
#endif
    }
  }

 public:

  void Setup() override {
    SimpleRWCommand::SetZeroTime();
    mongodb_ = make_shared<MongodbConnectionThreadPool>(loc_id_ == 0 ? mongodb_connection_ : 0);
    // Coroutine::CreateRun([&]() { 
    //   ExecutionHandler(this, mongodb_); 
    // });
    execution_thread = std::thread(ExecutionHandler, this, std::ref(mongodb_));
  }
  bool IsLeader() override {
    return loc_id_ == 0;
  }
  void Submit(const shared_ptr<Marshallable>& cmd) {
#ifdef MONGODB_DEBUG
    Log_info("%.2f Submit <%d, %d> loc_id %d", SimpleRWCommand::GetMsTimeElaps(), SimpleRWCommand::GetCmdID(cmd).first, SimpleRWCommand::GetCmdID(cmd).second, loc_id_);
#endif
    WAN_WAIT
    WAN_WAIT
#ifdef MONGODB_DEBUG
    Log_info("%.2f Before MongodbRequest <%d, %d>", SimpleRWCommand::GetMsTimeElaps(), SimpleRWCommand::GetCmdID(cmd).first, SimpleRWCommand::GetCmdID(cmd).second);
#endif
    mongodb_->MongodbRequest(cmd);
#ifdef MONGODB_DEBUG
    Log_info("%.2f After MongodbRequest <%d, %d>", SimpleRWCommand::GetMsTimeElaps(), SimpleRWCommand::GetCmdID(cmd).first, SimpleRWCommand::GetCmdID(cmd).second);
#endif
  }
  ~MongodbServer() {
    mongodb_->Close();
    execution_thread.join();
  }
};
}
