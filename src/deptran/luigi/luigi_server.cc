/**
 * LuigiServer: Standalone server implementation for Luigi protocol.
 */

#include "luigi_server.h"
#include "luigi_scheduler.h"
#include "luigi_rpc_setup.h"
#include "luigi_entry.h"

#include "deptran/config.h"
#include "rrr/rrr.hpp"

#include "lib/transport_request_handle.h"
#include "lib/helper_queue.h"
#include "lib/fasttransport.h"
#include "lib/common.h"
#include "benchmarks/sto/Interface.hh"
#include "benchmarks/common.h"
#include "benchmarks/benchmark_config.h"

#include <iostream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <mutex>

namespace janus {

//=============================================================================
// LuigiReceiver Implementation
//=============================================================================

LuigiReceiver::LuigiReceiver(const std::string& config_file)
    : config_(config_file) {
}

LuigiReceiver::~LuigiReceiver() {
    StopScheduler();
}

void LuigiReceiver::Register(abstract_db* db,
                              const std::map<int, abstract_ordered_index*>& tables) {
    db_ = db;
    tables_ = tables;
}

void LuigiReceiver::UpdateTableEntry(int table_id, abstract_ordered_index* table) {
    if (table_id > 0 && table) {
        tables_[table_id] = table;
    }
}

//=============================================================================
// TransportReceiver Interface
//=============================================================================

size_t LuigiReceiver::ReceiveRequest(uint8_t reqType, char* reqBuf, char* respBuf) {
    size_t respLen = 0;
    
    switch (reqType) {
        case luigi::kLuigiDispatchReqType:
            HandleDispatch(reqBuf, respBuf, respLen);
            break;
        case luigi::kLuigiStatusReqType:
            HandleStatusCheck(reqBuf, respBuf, respLen);
            break;
        case luigi::kOwdPingReqType:
            HandleOwdPing(reqBuf, respBuf, respLen);
            break;
        default:
            Log_warn("LuigiReceiver: Unrecognized request type: %d", reqType);
            break;
    }
    
    return respLen;
}

//=============================================================================
// Luigi Scheduler Management
//=============================================================================

void LuigiReceiver::InitScheduler(uint32_t partition_id) {
    if (scheduler_ != nullptr) {
        return;  // Already initialized
    }
    
    partition_id_ = partition_id;
    
    // Ensure minimal deptran Config exists
    janus::Config::CreateMinimalConfig();
    
    scheduler_ = new SchedulerLuigi();
    scheduler_->SetPartitionId(partition_id);
    
    // Set up read callback
    scheduler_->SetReadCallback(
        [this](int table_id, const std::string& key, std::string& value_out) -> bool {
            auto it = tables_.find(table_id);
            if (it == tables_.end() || it->second == nullptr) {
                return false;
            }
            return it->second->shard_get(lcdf::Str(key), value_out);
        }
    );
    
    // Set up write callback
    scheduler_->SetWriteCallback(
        [this](int table_id, const std::string& key, const std::string& value) -> bool {
            auto it = tables_.find(table_id);
            if (it == tables_.end() || it->second == nullptr) {
                return false;
            }
            try {
                it->second->shard_put(lcdf::Str(key), value);
                return true;
            } catch (...) {
                return false;
            }
        }
    );
    
    // Set up replication callback
    scheduler_->SetReplicationCallback(
        [this](const std::shared_ptr<LuigiLogEntry>& entry) -> bool {
            return ReplicateEntry(entry);
        }
    );
    
    scheduler_->Start();
    Log_info("Luigi scheduler initialized for partition %d", partition_id);
}

void LuigiReceiver::SetupRpc(rrr::Server* rpc_server,
                              rusty::Arc<rrr::PollThread> poll_thread,
                              const std::map<uint32_t, std::string>& shard_addresses) {
    if (scheduler_ == nullptr) {
        Log_error("SetupRpc: Luigi scheduler not initialized!");
        return;
    }
    
    if (rpc_setup_ != nullptr) {
        Log_warn("SetupRpc: Already set up");
        return;
    }
    
    rpc_setup_ = new LuigiRpcSetup();
    
    // Register RPC service
    if (rpc_server != nullptr) {
        bool ok = rpc_setup_->SetupService(rpc_server, scheduler_);
        if (!ok) {
            Log_error("SetupRpc: Failed to register service");
        }
    }
    
    // Connect to other shard leaders
    if (!shard_addresses.empty() && poll_thread) {
        int connected = rpc_setup_->ConnectToLeaders(
            shard_addresses, poll_thread, scheduler_);
        Log_info("Luigi RPC: connected to %d remote leaders", connected);
    }
}

void LuigiReceiver::StopScheduler() {
    if (rpc_setup_ != nullptr) {
        rpc_setup_->Shutdown();
        delete rpc_setup_;
        rpc_setup_ = nullptr;
    }
    
    if (scheduler_ != nullptr) {
        scheduler_->Stop();
        delete scheduler_;
        scheduler_ = nullptr;
        Log_info("Luigi scheduler stopped for partition %d", partition_id_);
    }
}

//=============================================================================
// Request Handlers
//=============================================================================

void LuigiReceiver::HandleDispatch(char* reqBuf, char* respBuf, size_t& respLen) {
    auto* req = reinterpret_cast<luigi::DispatchRequest*>(reqBuf);
    
    // Parse operations from request
    std::vector<LuigiOp> ops;
    char* data_ptr = req->ops_data;
    
    for (uint16_t i = 0; i < req->num_ops; i++) {
        LuigiOp op;
        
        // Read table_id (2 bytes)
        op.table_id = *reinterpret_cast<uint16_t*>(data_ptr);
        data_ptr += sizeof(uint16_t);
        
        // Read op_type (1 byte)
        op.op_type = *reinterpret_cast<uint8_t*>(data_ptr);
        data_ptr += sizeof(uint8_t);
        
        // Read key length (2 bytes)
        uint16_t klen = *reinterpret_cast<uint16_t*>(data_ptr);
        data_ptr += sizeof(uint16_t);
        
        // Read value length (2 bytes)
        uint16_t vlen = *reinterpret_cast<uint16_t*>(data_ptr);
        data_ptr += sizeof(uint16_t);
        
        // Read key
        op.key.assign(data_ptr, klen);
        data_ptr += klen;
        
        // Read value (for writes)
        if (vlen > 0) {
            op.value.assign(data_ptr, vlen);
            data_ptr += vlen;
        }
        
        ops.push_back(op);
    }
    
    // Extract involved shards
    std::vector<uint32_t> involved_shards;
    for (uint16_t i = 0; i < req->num_involved_shards && i < luigi::kMaxShards; i++) {
        involved_shards.push_back(req->involved_shards[i]);
    }
    
    // Prepare response
    auto* resp = reinterpret_cast<luigi::DispatchResponse*>(respBuf);
    resp->req_nr = req->req_nr;
    resp->txn_id = req->txn_id;
    respLen = sizeof(luigi::DispatchResponse);
    
    if (scheduler_ == nullptr) {
        Log_warn("Luigi scheduler not initialized, rejecting request");
        resp->status = luigi::kAbort;
        resp->commit_timestamp = 0;
        resp->num_results = 0;
        return;
    }
    
    uint64_t txn_id = req->txn_id;
    
    // Dispatch to scheduler with async callback
    scheduler_->LuigiDispatchFromRequest(
        txn_id,
        req->expected_time,
        ops,
        involved_shards,
        [this, txn_id](int status, uint64_t commit_ts, 
                        const std::vector<std::string>& read_results) {
            StoreResult(txn_id, status, commit_ts, read_results);
        }
    );
    
    // Return QUEUED immediately
    resp->status = luigi::kStatusQueued;
    resp->commit_timestamp = 0;
    resp->num_results = 0;
    
    Log_debug("Luigi dispatch queued txn %lu, expected_time %lu", txn_id, req->expected_time);
}

void LuigiReceiver::HandleStatusCheck(char* reqBuf, char* respBuf, size_t& respLen) {
    auto* req = reinterpret_cast<luigi::StatusRequest*>(reqBuf);
    auto* resp = reinterpret_cast<luigi::StatusResponse*>(respBuf);
    
    resp->req_nr = req->req_nr;
    resp->txn_id = req->txn_id;
    respLen = sizeof(luigi::StatusResponse);
    
    // Look up result
    {
        std::shared_lock<std::shared_mutex> lock(results_mutex_);
        auto it = completed_txns_.find(req->txn_id);
        
        if (it == completed_txns_.end()) {
            // Check if still pending
            if (scheduler_ != nullptr && scheduler_->HasPendingTxn(req->txn_id)) {
                resp->status = luigi::kStatusQueued;
            } else {
                resp->status = luigi::kStatusNotFound;
            }
            resp->commit_timestamp = 0;
            resp->num_results = 0;
            return;
        }
        
        // Found completed result
        const auto& result = it->second;
        resp->status = result.status;
        resp->commit_timestamp = result.commit_timestamp;
        resp->num_results = result.read_results.size();
        
        // Copy read results
        char* results_ptr = resp->results_data;
        for (const auto& val : result.read_results) {
            uint16_t vlen = val.size();
            memcpy(results_ptr, &vlen, sizeof(uint16_t));
            results_ptr += sizeof(uint16_t);
            memcpy(results_ptr, val.data(), vlen);
            results_ptr += vlen;
        }
    }
    
    // Remove result after retrieval
    if (resp->status == luigi::kStatusComplete || resp->status == luigi::kStatusAborted) {
        std::unique_lock<std::shared_mutex> lock(results_mutex_);
        completed_txns_.erase(req->txn_id);
    }
    
    Log_debug("Luigi status check txn %lu: status=%d", req->txn_id, resp->status);
}

void LuigiReceiver::HandleOwdPing(char* reqBuf, char* respBuf, size_t& respLen) {
    auto* req = reinterpret_cast<luigi::OwdPingRequest*>(reqBuf);
    auto* resp = reinterpret_cast<luigi::OwdPingResponse*>(respBuf);
    
    resp->req_nr = req->req_nr;
    resp->status = 0;  // OK
    respLen = sizeof(luigi::OwdPingResponse);
    
    Log_debug("OWD ping received, req_nr=%d, send_time=%lu", req->req_nr, req->send_time);
}

//=============================================================================
// Result Storage
//=============================================================================

void LuigiReceiver::StoreResult(uint64_t txn_id, int status, uint64_t commit_ts,
                                 const std::vector<std::string>& read_results) {
    std::unique_lock<std::shared_mutex> lock(results_mutex_);
    
    TxnResult result;
    result.status = (status == luigi::kOk) ? luigi::kStatusComplete : luigi::kStatusAborted;
    result.commit_timestamp = commit_ts;
    result.read_results = read_results;
    result.completion_time = std::chrono::steady_clock::now();
    
    completed_txns_[txn_id] = std::move(result);
    
    Log_debug("Luigi result stored for txn %lu: status=%d, commit_ts=%lu",
              txn_id, result.status, commit_ts);
    
    // Periodic cleanup
    static int cleanup_counter = 0;
    if (++cleanup_counter >= 100) {
        cleanup_counter = 0;
        lock.unlock();
        CleanupStaleResults();
    }
}

void LuigiReceiver::CleanupStaleResults(int ttl_seconds) {
    std::unique_lock<std::shared_mutex> lock(results_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    auto ttl = std::chrono::seconds(ttl_seconds);
    
    for (auto it = completed_txns_.begin(); it != completed_txns_.end(); ) {
        if (now - it->second.completion_time > ttl) {
            it = completed_txns_.erase(it);
        } else {
            ++it;
        }
    }
}

bool LuigiReceiver::ReplicateEntry(const std::shared_ptr<LuigiLogEntry>& entry) {
    // Get Paxos configuration
    auto& benchConfig = BenchmarkConfig::getInstance();
    
    if (!benchConfig.getIsReplicated()) {
        // No replication needed, just return success
        return true;
    }
    
    // TODO: Integrate with Mako's Paxos replication
    // For now, just log and return success
    Log_debug("Luigi entry replication requested for txn %lu (not implemented)", entry->tid);
    return true;
}

//=============================================================================
// LuigiServer Implementation
//=============================================================================

LuigiServer::LuigiServer(const std::string& config_file,
                         int client_shard_idx,
                         int server_shard_idx,
                         int partition_id)
    : config_(config_file),
      client_shard_idx_(client_shard_idx),
      server_shard_idx_(server_shard_idx),
      partition_id_(partition_id) {
    receiver_ = new LuigiReceiver(config_file);
}

LuigiServer::~LuigiServer() {
    if (receiver_) {
        receiver_->StopScheduler();
        delete receiver_;
        receiver_ = nullptr;
    }
}

void LuigiServer::Register(abstract_db* db,
                           mako::HelperQueue* queue,
                           mako::HelperQueue* queue_response,
                           const std::map<int, abstract_ordered_index*>& tables) {
    db_ = db;
    queue_ = queue;
    queue_response_ = queue_response;
    tables_ = tables;
    
    receiver_->Register(db, tables);
    
    // Initialize the scheduler with partition id
    receiver_->InitScheduler(partition_id_);
}

void LuigiServer::UpdateTable(int table_id, abstract_ordered_index* table) {
    if (table_id > 0 && table) {
        tables_[table_id] = table;
    }
    receiver_->UpdateTableEntry(table_id, table);
}

void LuigiServer::Run() {
    if (!queue_) {
        Log_warn("LuigiServer::Run() called but no queue registered");
        return;
    }
    
    // Initialize scheduler if not already done
    if (receiver_->GetScheduler() == nullptr) {
        receiver_->InitScheduler(partition_id_);
    }
    
    // Event loop similar to Mako's ShardServer::Run()
    while (true) {
        queue_->suspend();

        while (true) {
            erpc::ReqHandle *handle;
            size_t msg_size;
            if (!queue_->fetch_one_req(&handle, msg_size)) {
                break;
            }
            if (!handle) {
                Log_error("LuigiServer: invalid pointer in queue");
                continue;
            }

            // Cast to transport-agnostic interface
            mako::TransportRequestHandle* req_handle = 
                reinterpret_cast<mako::TransportRequestHandle*>(handle);

            // Process request through LuigiReceiver
            size_t msgLen = receiver_->ReceiveRequest(
                req_handle->GetRequestType(),
                req_handle->GetRequestBuffer(),
                req_handle->GetResponseBuffer());

            // Enqueue response
            req_handle->EnqueueResponse(msgLen);
        }

        if (queue_->should_stop()) {
            break;
        }
    }
    
    Log_info("LuigiServer::Run() exiting for partition %d", partition_id_);
}

void LuigiServer::SetupRpc(rrr::Server* rpc_server,
                           rusty::Arc<rrr::PollThread> poll_thread,
                           const std::map<uint32_t, std::string>& shard_addresses) {
    receiver_->SetupRpc(rpc_server, poll_thread, shard_addresses);
}

//=============================================================================
// Global Luigi Server Management
//=============================================================================

namespace {
    std::mutex g_luigi_servers_mu;
    std::vector<LuigiServer*> g_luigi_servers;
}

void RegisterLuigiServer(LuigiServer* server) {
    std::lock_guard<std::mutex> lock(g_luigi_servers_mu);
    g_luigi_servers.push_back(server);
}

void UnregisterLuigiServer(LuigiServer* server) {
    std::lock_guard<std::mutex> lock(g_luigi_servers_mu);
    auto it = std::find(g_luigi_servers.begin(), g_luigi_servers.end(), server);
    if (it != g_luigi_servers.end()) {
        g_luigi_servers.erase(it);
    }
}

SchedulerLuigi* GetLocalLuigiScheduler() {
    std::lock_guard<std::mutex> lock(g_luigi_servers_mu);
    if (!g_luigi_servers.empty()) {
        return g_luigi_servers[0]->GetScheduler();
    }
    return nullptr;
}

void SetupLuigiRpc() {
    auto& cfg = BenchmarkConfig::getInstance();
    
    // Only setup if Luigi mode is enabled
    if (!cfg.getUseLuigi()) {
        return;
    }
    
    auto& server_transports = cfg.getServerTransports();
    if (server_transports.empty()) {
        Log_info("setup_luigi_rpc: No server transports available, skipping");
        return;
    }
    
    // Get RPC server and poll thread from the first transport
    mako::FastTransport* transport = server_transports[0];
    if (!transport) {
        Log_warn("setup_luigi_rpc: First transport is null");
        return;
    }
    
    rrr::Server* rpc_server = transport->GetRpcServer();
    auto poll_thread_opt = transport->GetPollThread();
    
    if (!rpc_server) {
        Log_info("setup_luigi_rpc: No RPC server available (eRPC mode?), skipping");
        return;
    }
    
    if (poll_thread_opt.is_none()) {
        Log_warn("setup_luigi_rpc: No poll thread available");
        return;
    }
    
    rusty::Arc<rrr::PollThread> poll_thread = poll_thread_opt.unwrap();
    
    // Build shard addresses map: shard_id -> "host:port"
    std::map<uint32_t, std::string> shard_addresses;
    transport::Configuration* config = cfg.getConfig();
    if (config) {
        std::string cluster = cfg.getCluster();
        int cluster_role = mako::convertCluster(cluster);
        
        for (int shard_idx = 0; shard_idx < config->nshards; shard_idx++) {
            // Skip our own shard
            if (shard_idx == (int)cfg.getShardIndex()) {
                continue;
            }
            
            std::string host = config->shard(shard_idx, cluster_role).host;
            std::string port = config->shard(shard_idx, cluster_role).port;
            shard_addresses[shard_idx] = host + ":" + port;
        }
    }
    
    // Setup Luigi RPC for each registered Luigi server
    {
        std::lock_guard<std::mutex> lock(g_luigi_servers_mu);
        for (auto* server : g_luigi_servers) {
            server->SetupRpc(rpc_server, poll_thread.clone(), shard_addresses);
        }
        
        if (g_luigi_servers.empty() && cfg.getUseLuigi()) {
            Log_info("Single-shard Luigi mode: using direct local execution");
        }
    }
    
    Log_info("Luigi RPC setup complete: %zu shard addresses configured", shard_addresses.size());
}

}  // namespace janus
