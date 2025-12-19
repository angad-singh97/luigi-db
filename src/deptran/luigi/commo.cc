#include "commo.h"
#include "../command.h"
#include "../command_marshaler.h"
#include "../procedure.h"
#include "../rcc/graph.h"
#include "../rcc/graph_marshaler.h"
#include "../rcc_rpc.h"

namespace janus {

LuigiCommo::LuigiCommo(rusty::Option<rusty::Arc<PollThread>> poll)
    : Communicator(poll) {}

shared_ptr<IntEvent> LuigiCommo::SendDispatch(
    siteid_t site_id, parid_t par_id,
    rrr::i64 txn_id, rrr::i64 expected_time,
    rrr::i32 worker_id,
    const std::vector<rrr::i32> &involved_shards, const std::string &ops_data,
    rrr::i32 *status,
    rrr::i64 *commit_timestamp,
    std::string *results_data) {
  auto ret = Reactor::CreateSpEvent<IntEvent>();
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

    auto fu_result = proxy->async_Dispatch(
        txn_id, expected_time, worker_id,
        involved_shards, ops_data, fuattr);
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
  auto proxies = rpc_par_proxies_[par_id];

  for (auto &p : proxies) {
    if (p.first != site_id)
      continue;

    auto proxy = (LuigiProxy *)p.second;
    FutureAttr fuattr;
    fuattr.callback = [ret, status](rusty::Arc<Future> fu) {
      if (fu->get_error_code() != 0) {
        Log_info("Luigi OwdPing RPC error: %d", fu->get_error_code());
        return;
      }
      fu->get_reply() >> *status;
      ret->Set(1);
    };

    auto fu_result = proxy->async_OwdPing(send_time, fuattr);
    if (fu_result.is_err()) {
      Log_warn("Luigi OwdPing RPC failed: %d", fu_result.unwrap_err());
    }
  }

  return ret;
}

shared_ptr<IntEvent> LuigiCommo::SendDeadlinePropose(
    siteid_t site_id, parid_t par_id,
    rrr::i64 tid, rrr::i32 src_shard, rrr::i64 proposed_ts,
    rrr::i32 *status) {
  auto ret = Reactor::CreateSpEvent<IntEvent>();
  auto proxies = rpc_par_proxies_[par_id];

  for (auto &p : proxies) {
    if (p.first != site_id)
      continue;

    auto proxy = (LuigiProxy *)p.second;
    FutureAttr fuattr;
    fuattr.callback = [ret, status](rusty::Arc<Future> fu) {
      if (fu->get_error_code() != 0) {
        Log_info("Luigi DeadlinePropose RPC error: %d", fu->get_error_code());
        return;
      }
      fu->get_reply() >> *status;
      ret->Set(1);
    };

    auto fu_result = proxy->async_DeadlinePropose(
        tid, src_shard, proposed_ts, fuattr);
    if (fu_result.is_err()) {
      Log_warn("Luigi DeadlinePropose RPC failed: %d", fu_result.unwrap_err());
    }
  }

  return ret;
}

shared_ptr<IntEvent> LuigiCommo::SendDeadlineConfirm(
    siteid_t site_id, parid_t par_id,
    rrr::i64 tid, rrr::i32 src_shard, rrr::i64 agreed_ts,
    rrr::i32 *status) {
  auto ret = Reactor::CreateSpEvent<IntEvent>();
  auto proxies = rpc_par_proxies_[par_id];

  for (auto &p : proxies) {
    if (p.first != site_id)
      continue;

    auto proxy = (LuigiProxy *)p.second;
    FutureAttr fuattr;
    fuattr.callback = [ret, status](rusty::Arc<Future> fu) {
      if (fu->get_error_code() != 0) {
        Log_info("Luigi DeadlineConfirm RPC error: %d", fu->get_error_code());
        return;
      }
      fu->get_reply() >> *status;
      ret->Set(1);
    };

    auto fu_result = proxy->async_DeadlineConfirm(tid, src_shard, agreed_ts, fuattr);
    if (fu_result.is_err()) {
      Log_warn("Luigi DeadlineConfirm RPC failed: %d", fu_result.unwrap_err());
    }
  }

  return ret;
}

shared_ptr<IntEvent>
LuigiCommo::SendWatermarkExchange(siteid_t site_id, parid_t par_id,
                                  rrr::i32 src_shard,
                                  const std::vector<rrr::i64> &watermarks,
                                  rrr::i32 *status) {
  auto ret = Reactor::CreateSpEvent<IntEvent>();
  auto proxies = rpc_par_proxies_[par_id];

  for (auto &p : proxies) {
    if (p.first != site_id)
      continue;

    auto proxy = (LuigiProxy *)p.second;
    FutureAttr fuattr;
    fuattr.callback = [ret, status](rusty::Arc<Future> fu) {
      if (fu->get_error_code() != 0) {
        Log_info("Luigi WatermarkExchange RPC error: %d", fu->get_error_code());
        return;
      }
      fu->get_reply() >> *status;
      ret->Set(1);
    };

    auto fu_result = proxy->async_WatermarkExchange(src_shard, watermarks, fuattr);
    if (fu_result.is_err()) {
      Log_warn("Luigi WatermarkExchange RPC failed: %d",
               fu_result.unwrap_err());
    }
  }

  return ret;
}

void LuigiCommo::BroadcastOwdPing(
    int64_t send_time, const std::vector<uint32_t>& involved_shards) {
  auto config = Config::GetConfig();
  for (uint32_t shard : involved_shards) {
    auto sites = config->SitesByPartitionId(shard);
    if (sites.empty()) continue;
    siteid_t leader_site_id = config->LeaderSiteByPartitionId(shard).id;
    rrr::i32 status;
    SendOwdPing(leader_site_id, shard, send_time, &status);
  }
}

void LuigiCommo::BroadcastDeadlinePropose(
    uint64_t tid, int32_t src_shard, int64_t proposed_ts,
    const std::vector<uint32_t>& involved_shards) {
  auto config = Config::GetConfig();
  for (uint32_t shard : involved_shards) {
    auto sites = config->SitesByPartitionId(shard);
    if (sites.empty()) continue;
    siteid_t leader_site_id = config->LeaderSiteByPartitionId(shard).id;
    rrr::i32 status;
    SendDeadlinePropose(leader_site_id, shard, tid, src_shard, proposed_ts, &status);
  }
}

void LuigiCommo::BroadcastDeadlineConfirm(
    uint64_t tid, int32_t src_shard, int64_t agreed_ts,
    const std::vector<uint32_t>& involved_shards) {
  auto config = Config::GetConfig();
  for (uint32_t shard : involved_shards) {
    auto sites = config->SitesByPartitionId(shard);
    if (sites.empty()) continue;
    siteid_t leader_site_id = config->LeaderSiteByPartitionId(shard).id;
    rrr::i32 status;
    SendDeadlineConfirm(leader_site_id, shard, tid, src_shard, agreed_ts, &status);
  }
}

void LuigiCommo::BroadcastWatermarkExchange(
    int32_t src_shard, const std::vector<int64_t>& watermarks,
    const std::vector<uint32_t>& involved_shards) {
  auto config = Config::GetConfig();
  for (uint32_t shard : involved_shards) {
    auto sites = config->SitesByPartitionId(shard);
    if (sites.empty()) continue;
    siteid_t leader_site_id = config->LeaderSiteByPartitionId(shard).id;
    rrr::i32 status;
    SendWatermarkExchange(leader_site_id, shard, src_shard, watermarks, &status);
  }
}

} // namespace janus
