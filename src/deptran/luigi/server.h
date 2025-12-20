#pragma once

/**
 * LuigiServer: Main server class for Luigi protocol
 *
 * Manages scheduler, state machine, and RPC service.
 * Entry point is main() in server.cc.
 */

#include <memory>
#include <string>

#include "../__dep__.h" // For siteid_t
#include "commo.h"      // For LuigiRole

namespace rrr {
class Server;
}

namespace janus {

class SchedulerLuigi;
class LuigiServiceImpl;
class LuigiCommo;
class LuigiStateMachine;

/**
 * LuigiServer: Main server managing Luigi protocol execution.
 */
class LuigiServer {
public:
  explicit LuigiServer(int partition_id);
  ~LuigiServer();

  void Initialize(LuigiRole role, siteid_t site_id);
  void Start(const std::string &bind_addr);
  void Stop();

  void SetStateMachine(std::shared_ptr<LuigiStateMachine> sm) {
    state_machine_ = sm;
  }

  SchedulerLuigi *GetScheduler() { return scheduler_; }
  LuigiCommo *GetCommo() { return commo_.get(); }
  uint32_t GetPartitionId() const { return partition_id_; }

private:
  uint32_t partition_id_;
  int shard_idx_;

  // Core components
  SchedulerLuigi *scheduler_ = nullptr;
  std::shared_ptr<LuigiStateMachine> state_machine_;

  // RPC components
  std::shared_ptr<LuigiCommo> commo_;
  std::unique_ptr<LuigiServiceImpl> service_;
  std::unique_ptr<rrr::Server> rpc_server_;
};

} // namespace janus
