#pragma once

#include "__dep__.h"
#include "constants.h"
#include "../scheduler.h"
#include "../mongodb_kv_table_handler.h"
#include "../communicator.h"

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
    WAN_WAIT
    bool result = handler.Write(key, value);
    WAN_WAIT
    return result;
  }
  int Read(int key) {
    WAN_WAIT
    int result = handler.Read(key);
    WAN_WAIT
    return result;
  }
};

}