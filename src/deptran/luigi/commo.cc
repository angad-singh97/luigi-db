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
// Constructors
//=============================================================================

LuigiCommo::LuigiCommo(rusty::Option<rusty::Arc<PollThread>> poll)
    : Communicator(poll), role_(LuigiRole::COORDINATOR), shard_id_(0),
      site_id_(0) {
  ConnectByRole();
}

LuigiCommo::LuigiCommo(LuigiRole role, uint32_t shard_id, siteid_t site_id,
                       rusty::Option<rusty::Arc<PollThread>> poll)
    : Communicator(poll), role_(role), shard_id_(shard_id), site_id_(site_id) {
  ConnectByRole();
}

//=============================================================================
// Role-Aware Connection Setup
//=============================================================================

void LuigiCommo::ConnectByRole() {
  auto config = Config::GetConfig();
  if (!config) {
    Log_warn("LuigiCommo::ConnectByRole: Config not initialized");
    return;
  }

  Log_info("LuigiCommo connecting as %s, shard=%d, site=%d",
           role_ == LuigiRole::COORDINATOR ? "COORDINATOR"
           : role_ == LuigiRole::LEADER    ? "LEADER"
                                           : "FOLLOWER",
           shard_id_, site_id_);

  switch (role_) {
  case LuigiRole::COORDINATOR: {
    // Connect to leader of each shard
    for (auto par_id : config->GetAllPartitionIds()) {
      auto leader = config->LeaderSiteByPartitionId(par_id);
      auto result = ConnectToLuigiSite(leader);
      if (result.first == SUCCESS) {
        Log_info("Coordinator connected to shard %d leader (site %d)", par_id,
                 leader.id);
      }
    }
    break;
  }

  case LuigiRole::LEADER: {
    // Connect to other shard leaders
    for (auto par_id : config->GetAllPartitionIds()) {
      auto leader = config->LeaderSiteByPartitionId(par_id);
      if (leader.id != site_id_) { // Don't connect to self
        auto result = ConnectToLuigiSite(leader);
        if (result.first == SUCCESS) {
          Log_info("Leader connected to shard %d leader (site %d)", par_id,
                   leader.id);
        }
      }
    }
    // Connect to own followers
    for (auto &site : config->SitesByPartitionId(shard_id_)) {
      if (site.role != 0 && site.id != site_id_) { // follower, not self
        auto result = ConnectToLuigiSite(site);
        if (result.first == SUCCESS) {
          Log_info("Leader connected to follower (site %d)", site.id);
        }
      }
    }
    break;
  }

  case LuigiRole::FOLLOWER: {
    // Connect to own leader only
    auto leader = config->LeaderSiteByPartitionId(shard_id_);
    auto result = ConnectToLuigiSite(leader);
    if (result.first == SUCCESS) {
      Log_info("Follower connected to leader (site %d)", leader.id);
    }
    break;
  }
  }

  Log_info("LuigiCommo: %zu connections established", luigi_proxies_.size());
}

std::pair<int, LuigiProxy *>
LuigiCommo::ConnectToLuigiSite(Config::SiteInfo &site) {
  // Check if already connected
  auto it = luigi_proxies_.find(site.id);
  if (it != luigi_proxies_.end()) {
    return std::make_pair(SUCCESS, it->second);
  }

  // Use base Communicator connection
  auto result =
      ConnectToSite(site, std::chrono::milliseconds(CONNECT_TIMEOUT_MS));
  if (result.first != SUCCESS) {
    Log_warn("Failed to connect to site %d at %s", site.id,
             site.GetHostAddr().c_str());
    return std::make_pair(FAILURE, nullptr);
  }

  // Create LuigiProxy from the same client as ClassicProxy
  // The proxies share the underlying rrr::Client
  auto client_it = rpc_clients_.find(site.id);
  if (client_it == rpc_clients_.end()) {
    Log_warn("Client not found for site %d after ConnectToSite", site.id);
    return std::make_pair(FAILURE, nullptr);
  }

  auto luigi_proxy =
      new LuigiProxy(const_cast<rrr::Client *>(client_it->second.get()));
  luigi_proxies_[site.id] = luigi_proxy;

  return std::make_pair(SUCCESS, luigi_proxy);
}

//=============================================================================
// RPC Send Methods
//=============================================================================

shared_ptr<IntEvent> LuigiCommo::SendDispatch(
    siteid_t site_id, parid_t par_id, rrr::i64 txn_id, rrr::i64 expected_time,
    rrr::i32 worker_id, const std::vector<rrr::i32> &involved_shards,
    const std::string &ops_data, rrr::i32 *status, rrr::i64 *commit_timestamp,
    std::string *results_data) {
  auto ret = Reactor::CreateSpEvent<IntEvent>();

  // Try role-aware proxy first
  auto it = luigi_proxies_.find(site_id);
  if (it != luigi_proxies_.end()) {
    auto proxy = it->second;
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

  // Fallback to partition-based proxies
  auto proxies = rpc_par_proxies_[par_id];
  for (auto &p : proxies) {
    if (p.first != site_id)
      continue;

    auto proxy = (LuigiProxy *)p.second;
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
  }

  return ret;
}

shared_ptr<IntEvent> LuigiCommo::SendOwdPing(siteid_t site_id, parid_t par_id,
                                             rrr::i64 send_time,
                                             rrr::i32 *status) {
  auto ret = Reactor::CreateSpEvent<IntEvent>();

  auto it = luigi_proxies_.find(site_id);
  if (it != luigi_proxies_.end()) {
    auto proxy = it->second;
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

  auto proxies = rpc_par_proxies_[par_id];
  for (auto &p : proxies) {
    if (p.first != site_id)
      continue;
    auto proxy = (LuigiProxy *)p.second;
    FutureAttr fuattr;
    fuattr.callback = [ret, status](rusty::Arc<Future> fu) {
      if (fu->get_error_code() != 0)
        return;
      fu->get_reply() >> *status;
      ret->Set(1);
    };
    proxy->async_OwdPing(send_time, fuattr);
  }
  return ret;
}

shared_ptr<IntEvent>
LuigiCommo::SendDeadlinePropose(siteid_t site_id, parid_t par_id, rrr::i64 tid,
                                rrr::i32 src_shard, rrr::i64 proposed_ts,
                                rrr::i32 *status) {
  auto ret = Reactor::CreateSpEvent<IntEvent>();

  auto it = luigi_proxies_.find(site_id);
  if (it != luigi_proxies_.end()) {
    auto proxy = it->second;
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

  auto proxies = rpc_par_proxies_[par_id];
  for (auto &p : proxies) {
    if (p.first != site_id)
      continue;
    auto proxy = (LuigiProxy *)p.second;
    FutureAttr fuattr;
    fuattr.callback = [ret, status](rusty::Arc<Future> fu) {
      if (fu->get_error_code() != 0)
        return;
      fu->get_reply() >> *status;
      ret->Set(1);
    };
    proxy->async_DeadlinePropose(tid, src_shard, proposed_ts, fuattr);
  }
  return ret;
}

shared_ptr<IntEvent>
LuigiCommo::SendDeadlineConfirm(siteid_t site_id, parid_t par_id, rrr::i64 tid,
                                rrr::i32 src_shard, rrr::i64 agreed_ts,
                                rrr::i32 *status) {
  auto ret = Reactor::CreateSpEvent<IntEvent>();

  auto it = luigi_proxies_.find(site_id);
  if (it != luigi_proxies_.end()) {
    auto proxy = it->second;
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

  auto proxies = rpc_par_proxies_[par_id];
  for (auto &p : proxies) {
    if (p.first != site_id)
      continue;
    auto proxy = (LuigiProxy *)p.second;
    FutureAttr fuattr;
    fuattr.callback = [ret, status](rusty::Arc<Future> fu) {
      if (fu->get_error_code() != 0)
        return;
      fu->get_reply() >> *status;
      ret->Set(1);
    };
    proxy->async_DeadlineConfirm(tid, src_shard, agreed_ts, fuattr);
  }
  return ret;
}

shared_ptr<IntEvent> LuigiCommo::SendWatermarkExchange(
    siteid_t site_id, parid_t par_id, rrr::i32 src_shard,
    const std::vector<rrr::i64> &watermarks, rrr::i32 *status) {
  auto ret = Reactor::CreateSpEvent<IntEvent>();

  auto it = luigi_proxies_.find(site_id);
  if (it != luigi_proxies_.end()) {
    auto proxy = it->second;
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

  auto proxies = rpc_par_proxies_[par_id];
  for (auto &p : proxies) {
    if (p.first != site_id)
      continue;
    auto proxy = (LuigiProxy *)p.second;
    FutureAttr fuattr;
    fuattr.callback = [ret, status](rusty::Arc<Future> fu) {
      if (fu->get_error_code() != 0)
        return;
      fu->get_reply() >> *status;
      ret->Set(1);
    };
    proxy->async_WatermarkExchange(src_shard, watermarks, fuattr);
  }
  return ret;
}

//=============================================================================
// Synchronous Convenience Methods
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
// Broadcast Helpers
//=============================================================================

void LuigiCommo::BroadcastOwdPing(
    int64_t send_time, const std::vector<uint32_t> &involved_shards) {
  auto config = Config::GetConfig();
  for (uint32_t shard : involved_shards) {
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
    auto leader = config->LeaderSiteByPartitionId(shard);
    rrr::i32 status;
    SendWatermarkExchange(leader.id, shard, src_shard, watermarks, &status);
  }
}

} // namespace janus
