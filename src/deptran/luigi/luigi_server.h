#pragma once

/**
 * LuigiServer: Standalone server for Luigi timestamp-ordered execution
 * protocol.
 *
 * This is a complete separation from Mako's ShardReceiver, handling only
 * Luigi-specific request types while using Mako's eRPC transport.
 */

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "benchmarks/abstract_db.h"
#include "benchmarks/abstract_ordered_index.h"
#include "lib/fasttransport.h"
#include "lib/transport.h"

#include "luigi_common.h"
#include "luigi_entry.h"

// Forward declarations
namespace rrr {
class Server;
class PollThread;
} // namespace rrr
namespace rusty {
template <typename T> class Arc;
}
namespace mako {
class HelperQueue;
}

namespace janus {

class SchedulerLuigi;

/**
 * LuigiReceiver: Handles incoming Luigi protocol requests.
 *
 * Implements TransportReceiver interface for Mako's eRPC transport.
 * Manages Luigi scheduler lifecycle and request dispatching.
 */
class LuigiReceiver : public TransportReceiver {
public:
  explicit LuigiReceiver(const std::string &config_file);
  ~LuigiReceiver();

  //=========================================================================
  // Registration and Setup
  //=========================================================================

  /**
   * Register database and tables for Luigi operations.
   * Must be called before handling any requests.
   */
  void Register(abstract_db *db,
                const std::map<int, abstract_ordered_index *> &tables);

  /**
   * Update a single table entry (for dynamic table registration).
   */
  void UpdateTableEntry(int table_id, abstract_ordered_index *table);

  //=========================================================================
  // TransportReceiver Interface
  //=========================================================================

  size_t ReceiveRequest(uint8_t reqType, char *reqBuf, char *respBuf) override;
  void ReceiveResponse(uint8_t reqType, char *respBuf) override {}
  bool Blocked() override { return false; }

  //=========================================================================
  // Luigi Scheduler Management
  //=========================================================================

  /**
   * Initialize the Luigi scheduler for this partition.
   * Sets up read/write callbacks and replication hooks.
   */
  void InitScheduler(uint32_t partition_id);

  /**
   * Set up RPC connections to other shard leaders.
   * Required for multi-shard timestamp agreement.
   */
  void SetupRpc(rrr::Server *rpc_server,
                rusty::Arc<rrr::PollThread> poll_thread,
                const std::map<uint32_t, std::string> &shard_addresses);

  /**
   * Stop the scheduler and clean up resources.
   */
  void StopScheduler();

  /**
   * Get the Luigi scheduler (for local dispatch).
   */
  SchedulerLuigi *GetScheduler() { return scheduler_; }

protected:
  //=========================================================================
  // Request Handlers
  //=========================================================================

  void HandleDispatch(char *reqBuf, char *respBuf, size_t &respLen);
  void HandleStatusCheck(char *reqBuf, char *respBuf, size_t &respLen);
  void HandleOwdPing(char *reqBuf, char *respBuf, size_t &respLen);
  void HandleDeadlinePropose(char *reqBuf, char *respBuf, size_t &respLen);
  void HandleDeadlineConfirm(char *reqBuf, char *respBuf, size_t &respLen);
  void HandleWatermarkExchange(char *reqBuf, char *respBuf, size_t &respLen);

  //=========================================================================
  // Result Storage (for async polling)
  //=========================================================================

  struct TxnResult {
    int status;
    uint64_t commit_timestamp;
    std::vector<std::string> read_results;
    std::chrono::steady_clock::time_point completion_time;
  };

  void StoreResult(uint64_t txn_id, int status, uint64_t commit_ts,
                   const std::vector<std::string> &read_results);
  void CleanupStaleResults(int ttl_seconds = 60);

  /**
   * Replicate a Luigi entry via Paxos.
   * Called from scheduler's replication callback.
   */
  bool ReplicateEntry(const std::shared_ptr<LuigiLogEntry> &entry);

private:
  transport::Configuration config_;

  // Database layer
  abstract_db *db_ = nullptr;
  std::map<int, abstract_ordered_index *> tables_;

  // Luigi scheduler
  SchedulerLuigi *scheduler_ = nullptr;
  uint32_t partition_id_ = 0;

  // Async result storage
  std::unordered_map<uint64_t, TxnResult> completed_txns_;
  mutable std::shared_mutex results_mutex_;
};

/**
 * LuigiServer: Wrapper that creates and manages a LuigiReceiver.
 *
 * Similar to Mako's ShardServer, but only handles Luigi requests.
 */
class LuigiServer {
public:
  LuigiServer(const std::string &config_file, int client_shard_idx,
              int server_shard_idx, int partition_id);
  ~LuigiServer();

  /**
   * Register database, queues, and tables.
   */
  void Register(abstract_db *db, mako::HelperQueue *queue,
                mako::HelperQueue *queue_response,
                const std::map<int, abstract_ordered_index *> &tables);

  /**
   * Update a table entry.
   */
  void UpdateTable(int table_id, abstract_ordered_index *table);

  /**
   * Start the server's event loop.
   */
  void Run();

  /**
   * Set up Luigi RPC for multi-shard agreement.
   */
  void SetupRpc(rrr::Server *rpc_server,
                rusty::Arc<rrr::PollThread> poll_thread,
                const std::map<uint32_t, std::string> &shard_addresses);

  /**
   * Get the scheduler for local dispatch.
   */
  SchedulerLuigi *GetScheduler() {
    return receiver_ ? receiver_->GetScheduler() : nullptr;
  }

private:
  transport::Configuration config_;
  LuigiReceiver *receiver_ = nullptr;

  int client_shard_idx_;
  int server_shard_idx_;
  int partition_id_;

  abstract_db *db_ = nullptr;
  mako::HelperQueue *queue_ = nullptr;
  mako::HelperQueue *queue_response_ = nullptr;
  std::map<int, abstract_ordered_index *> tables_;
};

//=============================================================================
// Global Luigi Server Management
//=============================================================================

/**
 * Register a Luigi server instance for global management.
 * Used for setting up cross-shard RPC connections.
 */
void RegisterLuigiServer(LuigiServer *server);

/**
 * Unregister a Luigi server instance.
 */
void UnregisterLuigiServer(LuigiServer *server);

/**
 * Get the Luigi scheduler for local dispatch (multi-shard mode).
 * Returns nullptr if no Luigi servers are registered.
 */
SchedulerLuigi *GetLocalLuigiScheduler();

/**
 * Setup Luigi RPC for all registered servers.
 * This is the Luigi-equivalent of mako::setup_luigi_rpc().
 * Should be called after server transports are initialized.
 */
void SetupLuigiRpc();

} // namespace janus
