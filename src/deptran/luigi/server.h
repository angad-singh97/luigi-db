#pragma once

/**
 * LuigiServer: Main server class for Luigi protocol
 *
 * Similar to RaftServer - manages scheduler, state machine, and RPC service.
 */

#include <memory>
#include <string>

#include "lib/configuration.h"

namespace rrr {
class Server;
}

namespace janus {

class SchedulerLuigi;
class LuigiServiceImpl;
class LuigiCommo;
class LuigiStateMachine;
class LuigiExecutor;

/**
 * LuigiServer: Main server managing Luigi protocol execution.
 *
 * Analogous to RaftServer - coordinates scheduler, executor, state machine.
 */
class LuigiServer {
public:
  LuigiServer(const std::string &config_file, int shard_idx);
  ~LuigiServer();

  /**
   * Initialize the server components.
   */
  void Initialize();

  /**
   * Start the RRR server and event loop.
   */
  void Start(const std::string &bind_addr);

  /**
   * Stop the server.
   */
  void Stop();

  /**
   * Set the state machine (must be called before Start).
   */
  void SetStateMachine(LuigiStateMachine *sm) { state_machine_ = sm; }

  /**
   * Get the scheduler.
   */
  SchedulerLuigi *GetScheduler() { return scheduler_; }

  /**
   * Get the communicator.
   */
  LuigiCommo *GetCommo() { return commo_.get(); }

  /**
   * Get partition ID.
   */
  uint32_t GetPartitionId() const { return partition_id_; }

private:
  transport::Configuration config_;
  uint32_t partition_id_;
  int shard_idx_;

  // Core components
  SchedulerLuigi *scheduler_ = nullptr;
  LuigiExecutor *executor_ = nullptr;
  LuigiStateMachine *state_machine_ = nullptr;

  // RPC components
  std::shared_ptr<LuigiCommo> commo_;
  std::unique_ptr<LuigiServiceImpl> service_;
  std::unique_ptr<rrr::Server> rpc_server_;
};

} // namespace janus
