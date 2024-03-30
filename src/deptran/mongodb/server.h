#pragma once

#include "__dep__.h"
#include "constants.h"
#include "../scheduler.h"
#include "../mongodb_kv_table_handler.h"
#include "../mongodb_connection_thread_pool.h"
#include "../communicator.h"

namespace janus {

class MongodbServer : public TxLogServer {
  const int mongodb_connection_ = 100;

  // std::function<void(shared_ptr<Marshallable>)> callback_func_ = std::bind(&janus::MongodbServer::CommandEndCallback, this, std::placeholders::_1);
  shared_ptr<MongodbConnectionThreadPool> mongodb_ = make_shared<MongodbConnectionThreadPool>(mongodb_connection_);

 public:
  void Setup() override {
  }
  bool IsLeader() override {
    return loc_id_ == 0;
  }
  void Submit(const shared_ptr<Marshallable>& cmd) {
    // WAN_WAIT
    mongodb_->MongodbRequest(cmd);
    // WAN_WAIT
    RuleWitnessGC(cmd);
    app_next_(*cmd);
  }
  ~MongodbServer() {
    mongodb_->Close();
  }
};
}
