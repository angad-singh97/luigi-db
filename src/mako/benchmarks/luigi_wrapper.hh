#pragma once

#include "abstract_db.h"
#include "abstract_ordered_index.h"

#include "deptran/luigi/luigi_scheduler.h"
#include "deptran/luigi/luigi_executor.h"
#include "deptran/luigi/luigi_rpc_setup.h"
#include "deptran/luigi/luigi_owd.h"

#include "lib/fasttransport.h"
#include "benchmark_config.h"

#include <memory>
#include <map>
#include <mutex>
#include <vector>

class mbta_sharded_ordered_index;

/**
 * luigi_wrapper: Luigi DB backend for Mako benchmarks
 *
 * Implements abstract_db interface by delegating to SchedulerLuigi
 * and handling multi-shard timestamp-ordered execution.
 *
 * Usage:
 *   if (use_luigi) {
 *       db = new luigi_wrapper(config, open_tables);
 *   } else {
 *       db = new mbta_wrapper();
 *   }
 */
class luigi_wrapper : public abstract_db {
 public:
  /**
   * Constructor
   * 
   * @param config Transport configuration (for RPC setup)
   * @param open_tables Pre-opened tables indexed by table_id
   */
  luigi_wrapper(
      transport::Configuration* config,
      const std::map<int, abstract_ordered_index*>& open_tables);

  virtual ~luigi_wrapper();

  // ==================== abstract_db Implementation ====================

  virtual ssize_t txn_max_batch_size() const override { return -1; }

  virtual bool index_has_stable_put_memory() const override { return false; }

  virtual size_t sizeof_txn_object(uint64_t txn_flags) const override;

  virtual void do_txn_epoch_sync() const override {}

  virtual void do_txn_finish() const override {}

  virtual void thread_init(bool loader, int source = 0) override;

  virtual void thread_end() override {}

  virtual std::tuple<uint64_t, uint64_t, double>
  get_ntxn_persisted() const override { 
    return std::make_tuple(0, 0, 0.0); 
  }

  virtual void reset_ntxn_persisted() override {}

  /**
   * Create a new Luigi transaction entry and enqueue it to the scheduler
   */
  virtual void* new_txn(
      uint64_t txn_flags,
      str_arena& arena,
      void* buf,
      TxnProfileHint hint = HINT_DEFAULT) override;

  virtual counter_map
  get_txn_counters(void* txn) const override {
    return counter_map();
  }

  /**
   * For Luigi, commit happens asynchronously via the scheduler
   * This is a placeholder that returns true
   */
  virtual bool commit_txn(void* txn) override { return true; }

  /**
   * Same as commit_txn for Luigi
   */
  virtual bool commit_txn_no_paxos(void* txn) override { return true; }

  virtual void abort_txn(void* txn) override;

  virtual void abort_txn_local(void* txn) override;

  virtual void print_txn_debug(void* txn) const override {}

  virtual abstract_ordered_index*
  get_index_by_table_id(unsigned short table_id) override;

  virtual abstract_ordered_index*
  open_index(const std::string& name,
             size_t value_size_hint,
             bool mostly_append = false,
             bool use_hashtable = false) override;

  virtual void
  close_index(abstract_ordered_index* idx) override;

  virtual void preallocate_open_index() override {}

  virtual void init() override;

  virtual abstract_ordered_index*
  open_index(const std::string& name, int shard_index = -1) override;

  virtual mbta_sharded_ordered_index*
  open_sharded_index(const std::string& name) override {
    return nullptr;  // Not used in Luigi mode
  }

  // Shard-specific operations for multi-shard execution
  virtual void shard_abort_txn(void* txn) override;
  virtual int shard_validate() override;
  virtual void shard_install(uint32_t timestamp) override;
  virtual void shard_serialize_util(uint32_t timestamp) override;
  virtual void shard_unlock(bool committed) override;
  virtual void shard_reset() override;

  // ==================== Luigi-Specific Setup ====================

  /**
   * Set up Luigi RPC infrastructure
   * Call this after eRPC servers and helpers are initialized
   */
  void SetupLuigiRpc(
      rrr::Server* rpc_server,
      rusty::Arc<rrr::PollThread> poll_thread,
      const std::map<uint32_t, std::string>& shard_addresses);

  /**
   * Stop Luigi and clean up
   */
  void Stop();

  /**
   * Get the scheduler instance (for debugging/testing)
   */
  janus::SchedulerLuigi* GetScheduler() { return scheduler_.get(); }

  // ==================== Static Accessors ====================

  /**
   * Get the global Luigi wrapper instance (if running in Luigi mode)
   * Returns nullptr if not in Luigi mode
   */
  static luigi_wrapper* GetInstance();

 private:
  // Core Luigi components
  std::unique_ptr<janus::SchedulerLuigi> scheduler_;
  std::unique_ptr<janus::LuigiRpcSetup> rpc_setup_;
  std::unique_ptr<janus::LuigiExecutor> executor_;

  // Tables indexed by table_id
  std::map<int, abstract_ordered_index*> tables_by_id_;
  std::map<std::string, abstract_ordered_index*> tables_by_name_;
  std::mutex tables_mutex_;

  // Configuration
  transport::Configuration* config_;
  std::string cluster_;
  int local_shard_idx_;
  int num_shards_;

  // Transaction state (for thread-local access during execution)
  thread_local static janus::LuigiLogEntry* current_txn_;

  // Global instance (singleton)
  static luigi_wrapper* g_instance_;
  static std::mutex g_instance_mutex_;
};

#endif  // MAKO_BENCHMARKS_LUIGI_WRAPPER_HH
