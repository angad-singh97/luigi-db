#pragma once

#include "__dep__.h"
#include "constants.h"
#include "../scheduler.h"
#include "../mongodb_kv_table_handler.h"
#include "../mongodb_connection_thread_pool.h"
#include "../communicator.h"

namespace janus {

class MongodbServer : public TxLogServer {
  std::function<void(shared_ptr<Marshallable>)> callback_func_ = std::bind(&janus::TxLogServer::CommandEndCallback, this, std::placeholders::_1);
  MongodbConnectionThreadPool mongodb_(100, callback_func_);
 public:
  void Setup() {
  }
  bool IsLeader() override {
    return loc_id_ == 0;
  }
  void Submit(const shared_ptr<Marshallable>& cmd) {
    mongodb_.MongodbRequest(cmd);
  }
  ~MongodbServer() {
    mongodb_.Close();
  }
};

}