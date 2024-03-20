#pragma once

#include "__dep__.h"
#include "constants.h"
#include "../scheduler.h"
#include "../mongodb_kv_table_handler.h"


namespace janus {

class MongodbServer : public TxLogServer {
  MongodbKVTableHandler handler;
 public:
  void Setup() {
    // handler.clear();
  };
  bool IsLeader() override {
    return loc_id_ == 0;
  }
  bool Write(int key, int value) {
    // std::lock_guard<std::recursive_mutex> guard(mtx_);
    return handler.Write(key, value);
  }
  int Read(int key) {
    // std::lock_guard<std::recursive_mutex> guard(mtx_);
    return handler.Read(key);
  }
};

}