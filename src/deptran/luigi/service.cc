#include "service.h"
#include "scheduler.h"
#include "server.h"

#include "deptran/__dep__.h"
#include "luigi_common.h"

#include <algorithm>
#include <chrono>

namespace janus {

LuigiServiceImpl::LuigiServiceImpl(LuigiServer *server) : server_(server) {}

//=============================================================================
// RRR Handler Implementations
//=============================================================================

void LuigiServiceImpl::Dispatch(
    const rrr::i64 &txn_id, const rrr::i64 &expected_time,
    const rrr::i32 &worker_id, const std::vector<rrr::i32> &involved_shards,
    const std::string &ops_data, rrr::i32 *status, rrr::i64 *commit_timestamp,
    std::string *results_data, rrr::DeferredReply *defer) {

  Log_info("LuigiDispatch: received txn_id=%ld worker_id=%d op_size=%zu",
           txn_id, worker_id, ops_data.size());

  // Parse operations from binary data
  std::vector<LuigiOp> ops;
  const char *data_ptr = ops_data.data();
  const char *data_end = data_ptr + ops_data.size();

  while (data_ptr < data_end) {
    LuigiOp op;

    if (data_ptr + sizeof(uint16_t) > data_end)
      break;
    op.table_id = *reinterpret_cast<const uint16_t *>(data_ptr);
    data_ptr += sizeof(uint16_t);

    if (data_ptr + sizeof(uint8_t) > data_end)
      break;
    op.op_type = *reinterpret_cast<const uint8_t *>(data_ptr);
    data_ptr += sizeof(uint8_t);

    if (data_ptr + sizeof(uint16_t) > data_end)
      break;
    uint16_t klen = *reinterpret_cast<const uint16_t *>(data_ptr);
    data_ptr += sizeof(uint16_t);

    if (data_ptr + sizeof(uint16_t) > data_end)
      break;
    uint16_t vlen = *reinterpret_cast<const uint16_t *>(data_ptr);
    data_ptr += sizeof(uint16_t);

    if (data_ptr + klen > data_end)
      break;
    op.key.assign(data_ptr, klen);
    data_ptr += klen;

    if (vlen > 0) {
      if (data_ptr + vlen > data_end)
        break;
      op.value.assign(data_ptr, vlen);
      data_ptr += vlen;
    }

    ops.push_back(op);
  }

  // Convert involved shards
  std::vector<uint32_t> involved_shards_u32;
  for (auto shard : involved_shards) {
    involved_shards_u32.push_back(static_cast<uint32_t>(shard));
  }

  auto *scheduler = server_->GetScheduler();
  if (scheduler == nullptr) {
    Log_warn("Luigi scheduler not initialized");
    *status = luigi::kAbort;
    *commit_timestamp = 0;
    defer->reply();
    return;
  }

  // Keep pointer to defer for async reply after execution completes
  auto defer_ptr = defer;
  auto status_ptr = status;
  auto commit_timestamp_ptr = commit_timestamp;
  auto results_data_ptr = results_data;

  scheduler->LuigiDispatchFromRequest(
      txn_id, expected_time, ops, involved_shards_u32, worker_id,
      [defer_ptr, status_ptr, commit_timestamp_ptr, results_data_ptr,
       txn_id](int result_status, uint64_t commit_ts,
               const std::vector<std::string> &read_results) {
        // Set outputs and reply when execution completes
        Log_info("Service callback: txn=%ld status=%d ts=%lu, calling "
                 "defer->reply()",
                 txn_id, result_status, commit_ts);
        *status_ptr = result_status;
        *commit_timestamp_ptr = commit_ts;
        results_data_ptr->clear();
        for (const auto &r : read_results) {
          results_data_ptr->append(r);
        }
        defer_ptr->reply();
        Log_info("Service callback: txn=%ld defer->reply() returned", txn_id);
      });
}

void LuigiServiceImpl::OwdPing(const rrr::i64 &send_time, rrr::i32 *status,
                               rrr::DeferredReply *defer) {
  Log_info("OwdPing: received ping with send_time=%ld", send_time);
  *status = luigi::kOk;
  defer->reply();
}

void LuigiServiceImpl::DeadlinePropose(const rrr::i64 &tid,
                                       const rrr::i32 &src_shard,
                                       const rrr::i64 &proposed_ts,
                                       rrr::i32 *status,
                                       rrr::DeferredReply *defer) {

  Log_info("DeadlinePropose: tid=%ld from shard %d ts=%ld", tid, src_shard,
           proposed_ts);

  // Record the proposed timestamp from src_shard for this transaction
  auto *scheduler = server_->GetScheduler();
  if (scheduler != nullptr) {
    scheduler->HandleRemoteDeadlineProposal(tid, src_shard, proposed_ts, 1);
  }

  *status = luigi::kOk;
  defer->reply();
}

void LuigiServiceImpl::DeadlineConfirm(const rrr::i64 &tid,
                                       const rrr::i32 &src_shard,
                                       const rrr::i64 &agreed_ts,
                                       rrr::i32 *status,
                                       rrr::DeferredReply *defer) {

  Log_info("DeadlineConfirm: tid=%ld from shard %d ts=%ld", tid, src_shard,
           agreed_ts);

  // Record the confirmation (phase 2) from src_shard for this transaction
  auto *scheduler = server_->GetScheduler();
  if (scheduler != nullptr) {
    scheduler->HandleRemoteDeadlineConfirm(tid, src_shard, agreed_ts);
  }

  *status = luigi::kOk;
  defer->reply();
}

void LuigiServiceImpl::WatermarkExchange(
    const rrr::i32 &src_shard, const std::vector<rrr::i64> &watermarks,
    rrr::i32 *status, rrr::DeferredReply *defer) {

  *status = luigi::kOk;
  defer->reply();
}

//=============================================================================
// Result Storage
//=============================================================================

void LuigiServiceImpl::StoreResult(
    uint64_t txn_id, int status, uint64_t commit_ts,
    const std::vector<std::string> &read_results) {

  std::unique_lock<std::shared_mutex> lock(results_mutex_);

  TxnResult result;
  result.status =
      (status == luigi::kOk) ? luigi::kStatusComplete : luigi::kStatusAborted;
  result.commit_timestamp = commit_ts;
  result.read_results = read_results;
  result.completion_time = std::chrono::steady_clock::now();

  completed_txns_[txn_id] = std::move(result);

  Log_debug("Luigi result stored for txn %lu: status=%d, commit_ts=%lu", txn_id,
            result.status, commit_ts);
}

//=============================================================================
// PHASE 2: BATCH RPC HANDLERS
//=============================================================================

void LuigiServiceImpl::DeadlineBatchPropose(
    const std::vector<rrr::i64> &tids, const rrr::i32 &src_shard,
    const std::vector<rrr::i64> &proposed_timestamps, rrr::i32 *status,
    rrr::DeferredReply *defer) {

  Log_info("DeadlineBatchPropose: %zu proposals from shard %d", tids.size(),
           src_shard);

  // Process each proposal individually
  auto *scheduler = server_->GetScheduler();
  if (scheduler != nullptr) {
    for (size_t i = 0; i < tids.size(); i++) {
      scheduler->HandleRemoteDeadlineProposal(tids[i], src_shard,
                                              proposed_timestamps[i], 1);
    }
  }

  *status = luigi::kOk;
  defer->reply();
}

void LuigiServiceImpl::DeadlineBatchConfirm(
    const std::vector<rrr::i64> &tids, const rrr::i32 &src_shard,
    const std::vector<rrr::i64> &agreed_timestamps, rrr::i32 *status,
    rrr::DeferredReply *defer) {

  Log_info("DeadlineBatchConfirm: %zu confirmations from shard %d", tids.size(),
           src_shard);

  // Process each confirmation individually
  auto *scheduler = server_->GetScheduler();
  if (scheduler != nullptr) {
    for (size_t i = 0; i < tids.size(); i++) {
      scheduler->HandleRemoteDeadlineConfirm(tids[i], src_shard,
                                             agreed_timestamps[i]);
    }
  }

  *status = luigi::kOk;
  defer->reply();
}

} // namespace janus
