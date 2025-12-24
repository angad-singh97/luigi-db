#include "commo.h"
#include "../command.h"
#include "../command_marshaler.h"
#include "../config.h"
#include "../procedure.h"
#include "../rcc/graph.h"
#include "../rcc/graph_marshaler.h"

namespace janus {

//=============================================================================
// Constructor - uses base Communicator to connect to all sites
//=============================================================================

LuigiCommo::LuigiCommo(rusty::Option<rusty::Arc<PollThread>> poll)
    : Communicator(poll) {
  // Base Communicator constructor handles connecting to all sites
  Log_info("LuigiCommo: initialized with base Communicator connections");
}

//=============================================================================
// Helper to get or create LuigiProxy for a site
//=============================================================================

LuigiProxy *LuigiCommo::GetProxyForSite(siteid_t site_id) {
  // Check cache first
  auto it = luigi_proxies_.find(site_id);
  if (it != luigi_proxies_.end()) {
    return it->second;
  }

  // Create LuigiProxy on-demand from rpc_clients_
  auto client_it = rpc_clients_.find(site_id);
  if (client_it == rpc_clients_.end() || !client_it->second) {
    Log_warn("LuigiCommo: no client for site %d", site_id);
    return nullptr;
  }

  auto proxy =
      new LuigiProxy(const_cast<rrr::Client *>(client_it->second.get()));
  luigi_proxies_[site_id] = proxy;
  return proxy;
}

//=============================================================================
// Send Methods
//=============================================================================

shared_ptr<IntEvent> LuigiCommo::SendDispatch(
    siteid_t site_id, parid_t par_id, rrr::i64 txn_id, rrr::i64 expected_time,
    rrr::i32 worker_id, const std::vector<rrr::i32> &involved_shards,
    const std::string &ops_data, rrr::i32 *status, rrr::i64 *commit_timestamp,
    std::string *results_data) {

  auto ret = Reactor::CreateSpEvent<IntEvent>();
  auto proxy = GetProxyForSite(site_id);

  if (!proxy) {
    Log_warn("LuigiCommo::SendDispatch: no proxy for site %d", site_id);
    return ret;
  }

  FutureAttr fuattr;
  fuattr.callback = [ret, status, commit_timestamp,
                     results_data](rusty::Arc<Future> fu) {
    if (fu->get_error_code() != 0) {
      Log_info("Luigi Dispatch RPC error: %d", fu->get_error_code());
      return;
    }
    fu->get_reply() >> *status;
    fu->get_reply() >> *commit_timestamp;
    fu->get_reply() >> *results_data;
    ret->Set(1);
  };

  auto fu_result = proxy->async_Dispatch(txn_id, expected_time, worker_id,
                                         involved_shards, ops_data, fuattr);
  if (fu_result.is_err()) {
    Log_warn("Luigi Dispatch RPC failed: %d", fu_result.unwrap_err());
  }

  return ret;
}

shared_ptr<IntEvent> LuigiCommo::SendOwdPing(siteid_t site_id, parid_t par_id,
                                             rrr::i64 send_time,
                                             rrr::i32 *status) {
  auto ret = Reactor::CreateSpEvent<IntEvent>();
  auto proxy = GetProxyForSite(site_id);

  if (!proxy)
    return ret;

  FutureAttr fuattr;
  fuattr.callback = [ret, status](rusty::Arc<Future> fu) {
    if (fu->get_error_code() != 0)
      return;
    fu->get_reply() >> *status;
    ret->Set(1);
  };

  proxy->async_OwdPing(send_time, fuattr);
  return ret;
}

shared_ptr<IntEvent>
LuigiCommo::SendDeadlinePropose(siteid_t site_id, parid_t par_id, rrr::i64 tid,
                                rrr::i32 src_shard, rrr::i64 proposed_ts,
                                rrr::i32 *status) {

  auto ret = Reactor::CreateSpEvent<IntEvent>();
  auto proxy = GetProxyForSite(site_id);

  if (!proxy)
    return ret;

  FutureAttr fuattr;
  fuattr.callback = [ret, status](rusty::Arc<Future> fu) {
    if (fu->get_error_code() != 0)
      return;
    fu->get_reply() >> *status;
    ret->Set(1);
  };

  proxy->async_DeadlinePropose(tid, src_shard, proposed_ts, fuattr);
  return ret;
}

shared_ptr<IntEvent>
LuigiCommo::SendDeadlineConfirm(siteid_t site_id, parid_t par_id, rrr::i64 tid,
                                rrr::i32 src_shard, rrr::i64 agreed_ts,
                                rrr::i32 *status) {

  auto ret = Reactor::CreateSpEvent<IntEvent>();
  auto proxy = GetProxyForSite(site_id);

  if (!proxy)
    return ret;

  FutureAttr fuattr;
  fuattr.callback = [ret, status](rusty::Arc<Future> fu) {
    if (fu->get_error_code() != 0)
      return;
    fu->get_reply() >> *status;
    ret->Set(1);
  };

  proxy->async_DeadlineConfirm(tid, src_shard, agreed_ts, fuattr);
  return ret;
}

shared_ptr<IntEvent> LuigiCommo::SendWatermarkExchange(
    siteid_t site_id, parid_t par_id, rrr::i32 src_shard,
    const std::vector<rrr::i64> &watermarks, rrr::i32 *status) {

  auto ret = Reactor::CreateSpEvent<IntEvent>();
  auto proxy = GetProxyForSite(site_id);

  if (!proxy)
    return ret;

  FutureAttr fuattr;
  fuattr.callback = [ret, status](rusty::Arc<Future> fu) {
    if (fu->get_error_code() != 0)
      return;
    fu->get_reply() >> *status;
    ret->Set(1);
  };

  proxy->async_WatermarkExchange(src_shard, watermarks, fuattr);
  return ret;
}

//=============================================================================
// Sync Methods (blocking, for use from non-reactor threads)
// Uses fu->wait() pattern like hello_client.cc
//=============================================================================

bool LuigiCommo::OwdPingSync(parid_t shard_id, rrr::i64 send_time,
                             rrr::i32 *status) {
  // In Luigi, shard_id maps directly to site_id (one leader per shard)
  auto proxy = GetProxyForSite(shard_id);

  if (!proxy) {
    Log_warn("OwdPingSync: no proxy for leader of shard %d", shard_id);
    return false;
  }

  // Use async without callback, then wait on the future
  auto result = proxy->async_OwdPing(send_time);
  if (result.is_err()) {
    Log_warn("OwdPingSync: async_OwdPing failed for shard %d", shard_id);
    return false;
  }

  auto fu = result.unwrap();
  fu->wait();

  if (fu->get_error_code() == 0) {
    fu->get_reply() >> *status;
    return *status == 0;
  } else {
    *status = -1;
    return false;
  }
}

void LuigiCommo::OwdPingAsync(parid_t shard_id, rrr::i64 send_time,
                              OwdPingCallback callback) {
  // In Luigi, shard_id maps directly to site_id (one leader per shard)
  auto proxy = GetProxyForSite(shard_id);

  if (!proxy) {
    Log_warn("OwdPingAsync: no proxy for leader of shard %d", shard_id);
    callback(false, -1);
    return;
  }

  FutureAttr fuattr;
  fuattr.callback = [callback](rusty::Arc<Future> fu) {
    if (fu->get_error_code() != 0) {
      callback(false, -1);
      return;
    }
    rrr::i32 status;
    fu->get_reply() >> status;
    callback(true, status);
  };

  auto result = proxy->async_OwdPing(send_time, fuattr);
  if (result.is_err()) {
    Log_warn("OwdPingAsync: async_OwdPing failed for shard %d", shard_id);
    callback(false, -1);
  }
}

bool LuigiCommo::DispatchSync(parid_t shard_id, rrr::i64 txn_id,
                              rrr::i64 expected_time, rrr::i32 worker_id,
                              const std::vector<rrr::i32> &involved_shards,
                              const std::string &ops_data, rrr::i32 *status,
                              rrr::i64 *commit_timestamp,
                              std::string *results_data) {
  // In Luigi, shard_id maps directly to site_id (one leader per shard)
  auto proxy = GetProxyForSite(shard_id);

  if (!proxy) {
    Log_warn("DispatchSync: no proxy for leader of shard %d", shard_id);
    return false;
  }

  // Use async without callback, then wait on the future
  Log_debug("DispatchSync: calling async_Dispatch txn_id=%ld worker=%d", txn_id,
            worker_id);
  auto result = proxy->async_Dispatch(txn_id, expected_time, worker_id,
                                      involved_shards, ops_data);
  if (result.is_err()) {
    Log_warn("DispatchSync: async_Dispatch failed for shard %d", shard_id);
    return false;
  }

  auto fu = result.unwrap();
  Log_debug("DispatchSync: future created, waiting...");
  fu->wait();
  Log_debug("DispatchSync: future returned, error_code=%d",
            fu->get_error_code());

  if (fu->get_error_code() == 0) {
    fu->get_reply() >> *status;
    fu->get_reply() >> *commit_timestamp;
    fu->get_reply() >> *results_data;
    return *status == 0;
  } else {
    *status = -1;
    return false;
  }
}

//=============================================================================
// Async Dispatch (non-blocking, callback-based)
//=============================================================================

void LuigiCommo::DispatchAsync(parid_t shard_id, rrr::i64 txn_id,
                               rrr::i64 expected_time, rrr::i32 worker_id,
                               const std::vector<rrr::i32> &involved_shards,
                               const std::string &ops_data,
                               DispatchCallback callback) {
  // In Luigi, shard_id maps directly to site_id (one leader per shard)
  auto proxy = GetProxyForSite(shard_id);

  if (!proxy) {
    Log_warn("DispatchAsync: no proxy for leader of shard %d", shard_id);
    callback(false, -1, 0, "");
    return;
  }

  FutureAttr fuattr;
  fuattr.callback = [callback, txn_id, shard_id](rusty::Arc<Future> fu) {
    int error_code = fu->get_error_code();
    if (error_code != 0) {
      Log_warn("DispatchAsync callback: txn=%ld shard=%d RPC error=%d", txn_id,
               shard_id, error_code);
      callback(false, -1, 0, "");
      return;
    }
    rrr::i32 status;
    rrr::i64 commit_ts;
    std::string results;
    fu->get_reply() >> status;
    fu->get_reply() >> commit_ts;
    fu->get_reply() >> results;
    Log_debug("DispatchAsync callback: txn=%ld shard=%d success status=%d",
              txn_id, shard_id, status);
    callback(true, status, commit_ts, results);
  };

  Log_debug("DispatchAsync: calling async_Dispatch txn_id=%ld worker=%d "
            "shard=%d",
            txn_id, worker_id, shard_id);
  proxy->async_Dispatch(txn_id, expected_time, worker_id, involved_shards,
                        ops_data, fuattr);
}

//=============================================================================
// Broadcast Helpers - target appropriate entities
//=============================================================================

void LuigiCommo::BroadcastOwdPing(int64_t send_time,
                                  const std::vector<uint32_t> &shard_ids) {
  auto config = Config::GetConfig();
  for (uint32_t shard : shard_ids) {
    // In Luigi, shard_id maps directly to site_id (one leader per shard)
    rrr::i32 status;
    SendOwdPing(shard, shard, send_time, &status);
  }
}

void LuigiCommo::BroadcastDeadlinePropose(
    uint64_t tid, int32_t src_shard, int64_t proposed_ts,
    const std::vector<uint32_t> &involved_shards) {
  auto config = Config::GetConfig();
  for (uint32_t shard : involved_shards) {
    // Target: leaders of other shards only (exclude self)
    if (shard == static_cast<uint32_t>(src_shard))
      continue;

    // In Luigi, shard_id maps directly to site_id (one leader per shard)
    rrr::i32 status;
    SendDeadlinePropose(shard, shard, tid, src_shard, proposed_ts, &status);
  }
}

void LuigiCommo::BroadcastDeadlineConfirm(
    uint64_t tid, int32_t src_shard, int64_t agreed_ts,
    const std::vector<uint32_t> &involved_shards) {
  auto config = Config::GetConfig();
  for (uint32_t shard : involved_shards) {
    // Target: leaders only (exclude self)
    if (shard == static_cast<uint32_t>(src_shard))
      continue;

    // In Luigi, shard_id maps directly to site_id (one leader per shard)
    rrr::i32 status;
    SendDeadlineConfirm(shard, shard, tid, src_shard, agreed_ts, &status);
  }
}

void LuigiCommo::BroadcastWatermarkExchange(
    int32_t src_shard, const std::vector<int64_t> &watermarks,
    const std::vector<uint32_t> &involved_shards) {
  auto config = Config::GetConfig();
  for (uint32_t shard : involved_shards) {
    // In Luigi, shard_id maps directly to site_id (one leader per shard)
    rrr::i32 status;
    SendWatermarkExchange(shard, shard, src_shard, watermarks, &status);
  }
}

void LuigiCommo::BroadcastWatermarks(int32_t src_shard,
                                     const std::vector<int64_t> &watermarks) {
  // Send watermarks to coordinator for commit decision
  // TODO: Get coordinator site ID from config
  // For now, assume coordinator is at a special site ID or we broadcast to all
  // In multi-process mode, coordinator process will receive these

  auto config = Config::GetConfig();
  // Simplified: Send to all sites (coordinator will be among them)
  // In production, we'd have a dedicated coordinator site ID
  uint32_t num_partitions = config->GetNumPartition();
  for (uint32_t shard = 0; shard < num_partitions; ++shard) {
    if (shard != static_cast<uint32_t>(src_shard)) {
      rrr::i32 status;
      SendWatermarkExchange(shard, shard, src_shard, watermarks, &status);
    }
  }

  Log_debug("BroadcastWatermarks: shard=%d sent watermarks to all shards",
            src_shard);
}

//=============================================================================
// PHASE 2: BATCH BROADCAST IMPLEMENTATIONS
//=============================================================================

void LuigiCommo::BroadcastDeadlineBatchPropose(
    const std::vector<rrr::i64> &tids, int32_t src_shard,
    const std::vector<rrr::i64> &proposed_timestamps,
    const std::vector<rrr::i64> &watermarks,
    const std::vector<uint32_t> &involved_shards) {
  auto config = Config::GetConfig();
  for (uint32_t shard : involved_shards) {
    // Target: leaders of other shards only (exclude self)
    if (shard == static_cast<uint32_t>(src_shard))
      continue;

    // In Luigi, shard_id maps directly to site_id (one leader per shard)
    auto proxy = GetProxyForSite(shard);

    if (!proxy) {
      Log_warn(
          "LuigiCommo::BroadcastDeadlineBatchPropose: no proxy for site %d",
          shard);
      continue;
    }

    // Fire-and-forget async RPC (includes watermarks)
    rrr::FutureAttr fuattr;
    fuattr.callback = [](rusty::Arc<rrr::Future> fu) {
      // No-op callback for fire-and-forget
      if (fu->get_error_code() != 0) {
        Log_debug("DeadlineBatchPropose RPC error: %d", fu->get_error_code());
      }
    };

    auto future = proxy->async_DeadlineBatchPropose(
        tids, src_shard, proposed_timestamps, watermarks, fuattr);
    rrr::Future::safe_release(future);
  }
}

void LuigiCommo::BroadcastDeadlineBatchConfirm(
    const std::vector<rrr::i64> &tids, int32_t src_shard,
    const std::vector<rrr::i64> &agreed_timestamps,
    const std::vector<uint32_t> &involved_shards) {
  auto config = Config::GetConfig();
  for (uint32_t shard : involved_shards) {
    // Target: leaders of other shards only (exclude self)
    if (shard == static_cast<uint32_t>(src_shard))
      continue;

    // In Luigi, shard_id maps directly to site_id (one leader per shard)
    auto proxy = GetProxyForSite(shard);

    if (!proxy) {
      Log_warn(
          "LuigiCommo::BroadcastDeadlineBatchConfirm: no proxy for site %d",
          shard);
      continue;
    }

    // Fire-and-forget async RPC
    rrr::FutureAttr fuattr;
    fuattr.callback = [](rusty::Arc<rrr::Future> fu) {
      // No-op callback for fire-and-forget
      if (fu->get_error_code() != 0) {
        Log_debug("DeadlineBatchConfirm RPC error: %d", fu->get_error_code());
      }
    };

    auto future = proxy->async_DeadlineBatchConfirm(tids, src_shard,
                                                    agreed_timestamps, fuattr);
    rrr::Future::safe_release(future);
  }
}

} // namespace janus
