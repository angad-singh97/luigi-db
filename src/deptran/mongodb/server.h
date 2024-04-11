#pragma once

#include "__dep__.h"
#include "constants.h"
#include "../scheduler.h"
#include "../mongodb_kv_table_handler.h"
#include "../mongodb_connection_thread_pool.h"
#include "../communicator.h"

namespace janus {

class MongodbServer : public TxLogServer {
  const int mongodb_connection_ = 95; // [JetPack] seems mongodb community default maximum connextion between 95 * 5 and 100 * 5

  // shared_ptr<MongodbConnectionThreadPool> mongodb_ = make_shared<MongodbConnectionThreadPool>(mongodb_connection_);

  std::function<void(shared_ptr<Marshallable>)> callback_func_ = std::bind(&janus::MongodbServer::CommandEndCallback, this, std::placeholders::_1);
  shared_ptr<MongodbConnectionThreadPool> mongodb_ = make_shared<MongodbConnectionThreadPool>(mongodb_connection_, callback_func_);
  std::mutex ready_to_app_next_lock_;
  std::queue<shared_ptr<Marshallable>> ready_to_app_next_;
  void check_app_next() {
    lock_guard<std::mutex> guard(ready_to_app_next_lock_);
    while (ready_to_app_next_.size() > 0) {
      app_next_(*ready_to_app_next_.front());
      ready_to_app_next_.pop();
    }
  }
  void CommandEndCallback(const shared_ptr<Marshallable>& cmd) {
    RuleWitnessGC(cmd);
    lock_guard<std::mutex> guard(ready_to_app_next_lock_);
    ready_to_app_next_.push(cmd);
  }

 public:
  void Setup() override {
  }
  bool IsLeader() override {
    return loc_id_ == 0;
  }
  void Submit(const shared_ptr<Marshallable>& cmd) {
    WAN_WAIT
    mongodb_->MongodbRequest(cmd);
    WAN_WAIT
    check_app_next();
    // RuleWitnessGC(cmd);
    // app_next_(*cmd);
  }
  ~MongodbServer() {
    mongodb_->Close();
  }
};
}
