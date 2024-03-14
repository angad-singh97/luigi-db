#pragma once

#include "__dep__.h"
#include "constants.h"
#include "../coordinator.h"
#include "../mongodb_kv_table_handler.h"

namespace janus {

class CoordinatorMongodb : public Coordinator {
  MongodbKVTableHandler handler;
 public:
  CoordinatorMongodb(uint32_t coo_id,
                      int32_t benchmark,
                      ClientControlServiceImpl *ccsi,
                      uint32_t thread_id): Coordinator(coo_id, benchmark, ccsi, thread_id) {};
  ~CoordinatorMongodb() {}
  void DoTxAsync(TxRequest &req) override {}
  void Submit(shared_ptr<Marshallable> &cmd,
              const std::function<void()> &func = []() {},
              const std::function<void()> &exe_callback = []() {}) override;
  void Reset() override {}
  void Restart() override { verify(0); }
};

}