#include "commo.h"
#include "../command.h"
#include "../command_marshaler.h"
#include "../config.h"
#include "../procedure.h"
#include "../rcc/graph.h"
#include "../rcc/graph_marshaler.h"
#include "../rcc_rpc.h"

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

  auto fu_result = proxy->async_LuigiDispatch(
      txn_id, expected_time, worker_id, involved_shards, ops_data, fuattr);
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
// Sync Methods
//=============================================================================

bool LuigiCommo::OwdPingSync(parid_t shard_id, rrr::i64 send_time,
                             rrr::i32 *status) {
  auto config = Config::GetConfig();
  auto leader = config->LeaderSiteByPartitionId(shard_id);

  auto ev = SendOwdPing(leader.id, shard_id, send_time, status);
  if (!ev)
    return false;

  ev->Wait();
  return *status == 0;
}

bool LuigiCommo::DispatchSync(parid_t shard_id, rrr::i64 txn_id,
                              rrr::i64 expected_time, rrr::i32 worker_id,
                              const std::vector<rrr::i32> &involved_shards,
                              const std::string &ops_data, rrr::i32 *status,
                              rrr::i64 *commit_timestamp,
                              std::string *results_data) {
  auto config = Config::GetConfig();
  auto leader = config->LeaderSiteByPartitionId(shard_id);

  auto ev = SendDispatch(leader.id, shard_id, txn_id, expected_time, worker_id,
                         involved_shards, ops_data, status, commit_timestamp,
                         results_data);
  if (!ev)
    return false;

  ev->Wait();
  return *status == 0;
}

//=============================================================================
// Broadcast Helpers - target appropriate entities
//=============================================================================

void LuigiCommo::BroadcastOwdPing(int64_t send_time,
                                  const std::vector<uint32_t> &shard_ids) {
  auto config = Config::GetConfig();
  for (uint32_t shard : shard_ids) {
    // Target: leaders only
    auto leader = config->LeaderSiteByPartitionId(shard);
    rrr::i32 status;
    SendOwdPing(leader.id, shard, send_time, &status);
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

    auto leader = config->LeaderSiteByPartitionId(shard);
    rrr::i32 status;
    SendDeadlinePropose(leader.id, shard, tid, src_shard, proposed_ts, &status);
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

    auto leader = config->LeaderSiteByPartitionId(shard);
    rrr::i32 status;
    SendDeadlineConfirm(leader.id, shard, tid, src_shard, agreed_ts, &status);
  }
}

void LuigiCommo::BroadcastWatermarkExchange(
    int32_t src_shard, const std::vector<int64_t> &watermarks,
    const std::vector<uint32_t> &involved_shards) {
  auto config = Config::GetConfig();
  for (uint32_t shard : involved_shards) {
    // Target: other leaders + coordinator
    // For now, send to all leaders (coordinator will be handled separately)
    auto leader = config->LeaderSiteByPartitionId(shard);
    rrr::i32 status;
    SendWatermarkExchange(leader.id, shard, src_shard, watermarks, &status);
  }
}

} // namespace janus
