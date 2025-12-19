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
    siteid_t site_id, parid_t par_id, rrr::i32 target_server_id,
    rrr::i32 req_nr, rrr::i64 txn_id, rrr::i64 expected_time,
    rrr::i32 worker_id, rrr::i32 num_ops, rrr::i32 num_involved_shards,
    const std::vector<rrr::i32> &involved_shards, const std::string &ops_data,
    rrr::i32 *req_nr_out, rrr::i64 *txn_id_out, rrr::i32 *status,
    rrr::i64 *commit_timestamp, rrr::i32 *num_results,
    std::string *results_data) {
  auto ret = Reactor::CreateSpEvent<IntEvent>();
  auto proxies = rpc_par_proxies_[par_id];

  for (auto &p : proxies) {
    if (p.first != site_id)
      continue;

    auto proxy = (LuigiProxy *)p.second;
    FutureAttr fuattr;
    fuattr.callback = [ret, req_nr_out, txn_id_out, status, commit_timestamp,
                       num_results, results_data](rusty::Arc<Future> fu) {
      if (fu->get_error_code() != 0) {
        Log_info("Luigi Dispatch RPC error: %d", fu->get_error_code());
        return;
      }
      fu->get_reply() >> *req_nr_out;
      fu->get_reply() >> *txn_id_out;
      fu->get_reply() >> *status;
      fu->get_reply() >> *commit_timestamp;
      fu->get_reply() >> *num_results;
      fu->get_reply() >> *results_data;
      ret->Set(1);
    };

    auto fu_result = proxy->async_Dispatch(
        target_server_id, req_nr, txn_id, expected_time, worker_id, num_ops,
        num_involved_shards, involved_shards, ops_data, fuattr);
    if (fu_result.is_err()) {
      Log_warn("Luigi Dispatch RPC failed: %d", fu_result.unwrap_err());
    }
  }

  return ret;
}

shared_ptr<IntEvent> LuigiCommo::SendStatusCheck(
    siteid_t site_id, parid_t par_id, rrr::i32 target_server_id,
    rrr::i32 req_nr, rrr::i64 txn_id, rrr::i32 *req_nr_out,
    rrr::i64 *txn_id_out, rrr::i32 *status, rrr::i64 *commit_timestamp,
    rrr::i32 *num_results, std::string *results_data) {
  auto ret = Reactor::CreateSpEvent<IntEvent>();
  auto proxies = rpc_par_proxies_[par_id];

  for (auto &p : proxies) {
    if (p.first != site_id)
      continue;

    auto proxy = (LuigiProxy *)p.second;
    FutureAttr fuattr;
    fuattr.callback = [ret, req_nr_out, txn_id_out, status, commit_timestamp,
                       num_results, results_data](rusty::Arc<Future> fu) {
      if (fu->get_error_code() != 0) {
        Log_info("Luigi StatusCheck RPC error: %d", fu->get_error_code());
        return;
      }
      fu->get_reply() >> *req_nr_out;
      fu->get_reply() >> *txn_id_out;
      fu->get_reply() >> *status;
      fu->get_reply() >> *commit_timestamp;
      fu->get_reply() >> *num_results;
      fu->get_reply() >> *results_data;
      ret->Set(1);
    };

    auto fu_result =
        proxy->async_StatusCheck(target_server_id, req_nr, txn_id, fuattr);
    if (fu_result.is_err()) {
      Log_warn("Luigi StatusCheck RPC failed: %d", fu_result.unwrap_err());
    }
  }

  return ret;
}

shared_ptr<IntEvent> LuigiCommo::SendOwdPing(siteid_t site_id, parid_t par_id,
                                             rrr::i32 target_server_id,
                                             rrr::i32 req_nr,
                                             rrr::i64 send_time,
                                             rrr::i32 *req_nr_out,
                                             rrr::i32 *status) {
  auto ret = Reactor::CreateSpEvent<IntEvent>();
  auto proxies = rpc_par_proxies_[par_id];

  for (auto &p : proxies) {
    if (p.first != site_id)
      continue;

    auto proxy = (LuigiProxy *)p.second;
    FutureAttr fuattr;
    fuattr.callback = [ret, req_nr_out, status](rusty::Arc<Future> fu) {
      if (fu->get_error_code() != 0) {
        Log_info("Luigi OwdPing RPC error: %d", fu->get_error_code());
        return;
      }
      fu->get_reply() >> *req_nr_out;
      fu->get_reply() >> *status;
      ret->Set(1);
    };

    auto fu_result =
        proxy->async_OwdPing(target_server_id, req_nr, send_time, fuattr);
    if (fu_result.is_err()) {
      Log_warn("Luigi OwdPing RPC failed: %d", fu_result.unwrap_err());
    }
  }

  return ret;
}

shared_ptr<IntEvent> LuigiCommo::SendDeadlinePropose(
    siteid_t site_id, parid_t par_id, rrr::i32 target_server_id,
    rrr::i32 req_nr, rrr::i64 tid, rrr::i64 proposed_ts, rrr::i32 src_shard,
    rrr::i32 phase, rrr::i32 *req_nr_out, rrr::i64 *tid_out,
    rrr::i64 *proposed_ts_out, rrr::i32 *shard_id, rrr::i32 *status) {
  auto ret = Reactor::CreateSpEvent<IntEvent>();
  auto proxies = rpc_par_proxies_[par_id];

  for (auto &p : proxies) {
    if (p.first != site_id)
      continue;

    auto proxy = (LuigiProxy *)p.second;
    FutureAttr fuattr;
    fuattr.callback = [ret, req_nr_out, tid_out, proposed_ts_out, shard_id,
                       status](rusty::Arc<Future> fu) {
      if (fu->get_error_code() != 0) {
        Log_info("Luigi DeadlinePropose RPC error: %d", fu->get_error_code());
        return;
      }
      fu->get_reply() >> *req_nr_out;
      fu->get_reply() >> *tid_out;
      fu->get_reply() >> *proposed_ts_out;
      fu->get_reply() >> *shard_id;
      fu->get_reply() >> *status;
      ret->Set(1);
    };

    auto fu_result = proxy->async_DeadlinePropose(
        target_server_id, req_nr, tid, proposed_ts, src_shard, phase, fuattr);
    if (fu_result.is_err()) {
      Log_warn("Luigi DeadlinePropose RPC failed: %d", fu_result.unwrap_err());
    }
  }

  return ret;
}

shared_ptr<IntEvent> LuigiCommo::SendDeadlineConfirm(
    siteid_t site_id, parid_t par_id, rrr::i32 target_server_id,
    rrr::i32 req_nr, rrr::i64 tid, rrr::i32 src_shard, rrr::i64 new_ts,
    rrr::i32 *req_nr_out, rrr::i32 *status) {
  auto ret = Reactor::CreateSpEvent<IntEvent>();
  auto proxies = rpc_par_proxies_[par_id];

  for (auto &p : proxies) {
    if (p.first != site_id)
      continue;

    auto proxy = (LuigiProxy *)p.second;
    FutureAttr fuattr;
    fuattr.callback = [ret, req_nr_out, status](rusty::Arc<Future> fu) {
      if (fu->get_error_code() != 0) {
        Log_info("Luigi DeadlineConfirm RPC error: %d", fu->get_error_code());
        return;
      }
      fu->get_reply() >> *req_nr_out;
      fu->get_reply() >> *status;
      ret->Set(1);
    };

    auto fu_result = proxy->async_DeadlineConfirm(target_server_id, req_nr, tid,
                                                  src_shard, new_ts, fuattr);
    if (fu_result.is_err()) {
      Log_warn("Luigi DeadlineConfirm RPC failed: %d", fu_result.unwrap_err());
    }
  }

  return ret;
}

shared_ptr<IntEvent>
LuigiCommo::SendWatermarkExchange(siteid_t site_id, parid_t par_id,
                                  rrr::i32 target_server_id, rrr::i32 req_nr,
                                  rrr::i32 src_shard, rrr::i32 num_watermarks,
                                  const std::vector<rrr::i64> &watermarks,
                                  rrr::i32 *req_nr_out, rrr::i32 *status) {
  auto ret = Reactor::CreateSpEvent<IntEvent>();
  auto proxies = rpc_par_proxies_[par_id];

  for (auto &p : proxies) {
    if (p.first != site_id)
      continue;

    auto proxy = (LuigiProxy *)p.second;
    FutureAttr fuattr;
    fuattr.callback = [ret, req_nr_out, status](rusty::Arc<Future> fu) {
      if (fu->get_error_code() != 0) {
        Log_info("Luigi WatermarkExchange RPC error: %d", fu->get_error_code());
        return;
      }
      fu->get_reply() >> *req_nr_out;
      fu->get_reply() >> *status;
      ret->Set(1);
    };

    auto fu_result =
        proxy->async_WatermarkExchange(target_server_id, req_nr, src_shard,
                                       num_watermarks, watermarks, fuattr);
    if (fu_result.is_err()) {
      Log_warn("Luigi WatermarkExchange RPC failed: %d",
               fu_result.unwrap_err());
    }
  }

  return ret;
}

} // namespace janus
