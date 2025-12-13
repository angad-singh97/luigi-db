#include "luigi_wrapper.hh"

#include <chrono>
#include <functional>

#include "deptran/__dep__.h"
#include "benchmark_config.h"
#include "lib/common.h"
#include "sto/Transaction.hh"

// Thread-local storage for current transaction
thread_local janus::LuigiLogEntry* luigi_wrapper::current_txn_ = nullptr;

// Static instance storage
luigi_wrapper* luigi_wrapper::g_instance_ = nullptr;
std::mutex luigi_wrapper::g_instance_mutex_;

//=============================================================================
// Static Accessors
//=============================================================================

luigi_wrapper* luigi_wrapper::GetInstance() {
  std::lock_guard<std::mutex> lock(g_instance_mutex_);
  return g_instance_;
}

//=============================================================================
// Constructor / Destructor
//=============================================================================

luigi_wrapper::luigi_wrapper(
    transport::Configuration* config,
    const std::map<int, abstract_ordered_index*>& open_tables)
    : config_(config),
      local_shard_idx_(BenchmarkConfig::getInstance().getShardIndex()),
      num_shards_(config ? config->nshards : 1) {
  
  // Copy tables into our internal maps
  for (const auto& [table_id, table] : open_tables) {
    if (table) {
      tables_by_id_[table_id] = table;
      // Note: we don't have table names here, would need to extract from table if needed
    }
  }
  
  // Create scheduler
  scheduler_ = std::make_unique<janus::SchedulerLuigi>();
  
  // Create executor
  executor_ = std::make_unique<janus::LuigiExecutor>();
  executor_->SetPartitionId(local_shard_idx_);
  executor_->SetScheduler(scheduler_.get());
  
  // Create RPC setup helper
  rpc_setup_ = std::make_unique<janus::LuigiRpcSetup>();
  
  // Register this instance globally
  {
    std::lock_guard<std::mutex> lock(g_instance_mutex_);
    g_instance_ = this;
  }
  
  Notice("Luigi wrapper initialized: shard %d/%d, %zu tables",
         local_shard_idx_, num_shards_, tables_by_id_.size());
}

luigi_wrapper::~luigi_wrapper() {
  // Unregister global instance
  {
    std::lock_guard<std::mutex> lock(g_instance_mutex_);
    if (g_instance_ == this) {
      g_instance_ = nullptr;
    }
  }
  Stop();
}

//=============================================================================
// abstract_db Implementation
//=============================================================================

size_t luigi_wrapper::sizeof_txn_object(uint64_t txn_flags) const {
  // Luigi transactions are small - just need space for a pointer
  return sizeof(void*);
}

void luigi_wrapper::thread_init(bool loader, int source) {
  // Initialize thread-local state for Luigi
  // This would be called for each worker thread
}

void* luigi_wrapper::new_txn(
    uint64_t txn_flags,
    str_arena& arena,
    void* buf,
    TxnProfileHint hint) {
  
  // For Luigi, we don't actually execute here - we just return a placeholder
  // The actual transaction creation happens when the transaction is dispatched
  // through the Luigi scheduler.
  //
  // This is called from the benchmark runner, which will populate the transaction
  // with operations and then call commit_txn() to actually submit it.
  
  if (!buf) return nullptr;
  
  // Store a pointer to a new (empty) transaction entry
  auto* entry = new janus::LuigiLogEntry();
  *static_cast<janus::LuigiLogEntry**>(buf) = entry;
  
  Notice("new_txn: created entry at %p", entry);
  return buf;
}

void luigi_wrapper::abort_txn(void* txn) {
  if (!txn) return;
  
  auto* entry_ptr = static_cast<janus::LuigiLogEntry**>(txn);
  auto* entry = *entry_ptr;
  
  if (entry) {
    delete entry;
    *entry_ptr = nullptr;
  }
}

void luigi_wrapper::abort_txn_local(void* txn) {
  abort_txn(txn);
}

abstract_ordered_index*
luigi_wrapper::get_index_by_table_id(unsigned short table_id) {
  std::lock_guard<std::mutex> lock(tables_mutex_);
  auto it = tables_by_id_.find(table_id);
  if (it != tables_by_id_.end()) {
    return it->second;
  }
  return nullptr;
}

abstract_ordered_index*
luigi_wrapper::open_index(const std::string& name,
                          size_t value_size_hint,
                          bool mostly_append,
                          bool use_hashtable) {
  // In Luigi mode, tables should already be opened
  std::lock_guard<std::mutex> lock(tables_mutex_);
  auto it = tables_by_name_.find(name);
  if (it != tables_by_name_.end()) {
    return it->second;
  }
  return nullptr;
}

void luigi_wrapper::close_index(abstract_ordered_index* idx) {
  // Tables are managed by the benchmark, not by us
}

void luigi_wrapper::init() {
  // Initialize the scheduler
  if (scheduler_) {
    scheduler_->Start();
    Notice("Luigi scheduler started");
  }
  
  // Initialize OWD service for multi-shard mode
  if (num_shards_ > 1 && config_) {
    auto& owd = mako::luigi::LuigiOWD::getInstance();
    owd.init(
        config_->configFile,
        BenchmarkConfig::getInstance().getCluster(),
        local_shard_idx_,
        num_shards_);
    owd.start();
    Notice("Luigi OWD service started");
  }
}

abstract_ordered_index*
luigi_wrapper::open_index(const std::string& name, int shard_index) {
  return open_index(name);
}

//=============================================================================
// Shard Operations (for direct execution during agreement phase)
//=============================================================================

void luigi_wrapper::shard_abort_txn(void* txn) {
  // In Luigi, this is called to abort speculative work
  // For now, we don't track this
}

int luigi_wrapper::shard_validate() {
  // Validate the current transaction
  // For single-shard execution, this would check for conflicts
  return 0;  // Success
}

void luigi_wrapper::shard_install(uint32_t timestamp) {
  // Install transaction at the given timestamp
}

void luigi_wrapper::shard_serialize_util(uint32_t timestamp) {
  // Serialize utility state
}

void luigi_wrapper::shard_unlock(bool committed) {
  // Unlock resources
}

void luigi_wrapper::shard_reset() {
  // Reset transaction state
}

//=============================================================================
// Luigi-Specific Setup
//=============================================================================

void luigi_wrapper::SetupLuigiRpc(
    rrr::Server* rpc_server,
    rusty::Arc<rrr::PollThread> poll_thread,
    const std::map<uint32_t, std::string>& shard_addresses) {
  
  if (!scheduler_ || !rpc_setup_) {
    Warning("SetupLuigiRpc: scheduler or rpc_setup not initialized");
    return;
  }
  
  // Register the RPC service
  if (!rpc_setup_->SetupService(rpc_server, scheduler_.get())) {
    Warning("SetupLuigiRpc: failed to setup service");
    return;
  }
  
  // Connect to other shard leaders
  int connected = rpc_setup_->ConnectToLeaders(
      shard_addresses, poll_thread, scheduler_.get());
  
  Notice("Luigi RPC setup complete: connected to %d shards", connected);
}

void luigi_wrapper::Stop() {
  if (scheduler_) {
    scheduler_->Stop();
    Notice("Luigi scheduler stopped");
  }
  
  if (rpc_setup_) {
    rpc_setup_->Shutdown();
    Notice("Luigi RPC shutdown");
  }
  
  // Stop OWD service
  if (num_shards_ > 1) {
    auto& owd = mako::luigi::LuigiOWD::getInstance();
    if (owd.isRunning()) {
      owd.stop();
      Notice("Luigi OWD service stopped");
    }
  }
}
