#pragma once

#include "__dep__.h"
#include "constants.h"
#include "../scheduler.h"
#include "../mongodb_kv_table_handler.h"


namespace janus {

class MongodbServer : public TxLogServer {
  // MongodbKVTableHandler handler;
 public:
  void Setup() {};
  bool IsLeader() override {
    return loc_id_ == 0;
  }
};

}