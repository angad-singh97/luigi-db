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

void LuigiServiceImpl::Dispatch(const rrr::i32 &target_server_id,
                            const rrr::i32 &req_nr, const rrr::i64 &txn_id,
                            const rrr::i64 &expected_time,
                            const rrr::i32 &worker_id, const rrr::i32 &num_ops,
                            const rrr::i32 &num_involved_shards,
                            const std::vector<rrr::i32> &involved_shards,
                            const std::string &ops_data, rrr::i32 *req_nr_out,
                            rrr::i64 *txn_id_out, rrr::i32 *status,
                            rrr::i64 *commit_timestamp, rrr::i32 *num_results,
                            std::string *results_data,
                            rrr::DeferredReply *defer) {

  *req_nr_out = req_nr;
  *txn_id_out = txn_id;

  // Parse operations from binary data
  std::vector<LuigiOp> ops;
  const char *data_ptr = ops_data.data();
  const char *data_end = data_ptr + ops_data.size();

  for (int i = 0; i < num_ops && data_ptr < data_end; i++) {
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
    *num_results = 0;
    defer->reply();
    return;
  }

  scheduler->LuigiDispatchFromRequest(
      txn_id, expected_time, ops, involved_shards_u32, worker_id,
      [this, txn_id](int result_status, uint64_t commit_ts,
                     const std::vector<std::string> &read_results) {
        StoreResult(txn_id, result_status, commit_ts, read_results);
      });

  *status = luigi::kStatusQueued;
  *commit_timestamp = 0;
  *num_results = 0;
  defer->reply();
}

void LuigiServiceImpl::StatusCheck(const rrr::i32 &target_server_id,
                               const rrr::i32 &req_nr, const rrr::i64 &txn_id,
                               rrr::i32 *req_nr_out, rrr::i64 *txn_id_out,
                               rrr::i32 *status, rrr::i64 *commit_timestamp,
                               rrr::i32 *num_results, std::string *results_data,
                               rrr::DeferredReply *defer) {

  *req_nr_out = req_nr;
  *txn_id_out = txn_id;

  {
    std::shared_lock<std::shared_mutex> lock(results_mutex_);
    auto it = completed_txns_.find(txn_id);

    if (it == completed_txns_.end()) {
      auto *scheduler = server_->GetScheduler();
      if (scheduler != nullptr && scheduler->HasPendingTxn(txn_id)) {
        *status = luigi::kStatusQueued;
      } else {
        *status = luigi::kStatusNotFound;
      }
      *commit_timestamp = 0;
      *num_results = 0;
      defer->reply();
      return;
    }

    const auto &result = it->second;
    *status = result.status;
    *commit_timestamp = result.commit_timestamp;
    *num_results = result.read_results.size();

    results_data->clear();
    for (const auto &read_result : result.read_results) {
      results_data->append(read_result);
    }
  }

  defer->reply();
}

void LuigiServiceImpl::OwdPing(const rrr::i32 &target_server_id,
                           const rrr::i32 &req_nr, const rrr::i64 &send_time,
                           rrr::i32 *req_nr_out, rrr::i32 *status,
                           rrr::DeferredReply *defer) {

  *req_nr_out = req_nr;
  *status = luigi::kOk;
  defer->reply();
}

void LuigiServiceImpl::DeadlinePropose(const rrr::i32 &target_server_id,
                                   const rrr::i32 &req_nr, const rrr::i64 &tid,
                                   const rrr::i64 &proposed_ts,
                                   const rrr::i32 &src_shard,
                                   const rrr::i32 &phase, rrr::i32 *req_nr_out,
                                   rrr::i64 *tid_out, rrr::i64 *proposed_ts_out,
                                   rrr::i32 *shard_id, rrr::i32 *status,
                                   rrr::DeferredReply *defer) {

  *req_nr_out = req_nr;
  *tid_out = tid;
  *proposed_ts_out = 0;
  *shard_id = target_server_id;
  *status = luigi::kOk;
  defer->reply();
}

void LuigiServiceImpl::DeadlineConfirm(const rrr::i32 &target_server_id,
                                   const rrr::i32 &req_nr, const rrr::i64 &tid,
                                   const rrr::i32 &src_shard,
                                   const rrr::i64 &new_ts, rrr::i32 *req_nr_out,
                                   rrr::i32 *status,
                                   rrr::DeferredReply *defer) {

  *req_nr_out = req_nr;
  *status = luigi::kOk;
  defer->reply();
}

void LuigiServiceImpl::WatermarkExchange(const rrr::i32 &target_server_id,
                                     const rrr::i32 &req_nr,
                                     const rrr::i32 &src_shard,
                                     const rrr::i32 &num_watermarks,
                                     const std::vector<rrr::i64> &watermarks,
                                     rrr::i32 *req_nr_out, rrr::i32 *status,
                                     rrr::DeferredReply *defer) {

  *req_nr_out = req_nr;
  *status = luigi::kOk;
  defer->reply();
}

//=============================================================================
// Result Storage
//=============================================================================

void LuigiServiceImpl::StoreResult(uint64_t txn_id, int status, uint64_t commit_ts,
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

} // namespace janus
