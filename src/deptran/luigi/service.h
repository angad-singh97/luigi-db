#pragma once

/**
 * LuigiService: RPC request handler for Luigi protocol
 *
 * Implements the LuigiService interface generated from luigi.rpc.
 * Handles incoming RPC requests and dispatches to the scheduler.
 */

#include <chrono>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "luigi.h" // Generated RRR service
#include "luigi_common.h"

namespace janus {

class LuigiServer; // Forward declaration

/**
 * LuigiServiceImpl: Handles incoming Luigi protocol RPC requests.
 *
 * Similar to RaftService - implements RPC handlers only.
 */
class LuigiServiceImpl : public LuigiService {
public:
  explicit LuigiServiceImpl(LuigiServer *server);
  ~LuigiServiceImpl() = default;

  //===========================================================================
  // RRR LuigiService Interface Implementation
  //===========================================================================

  void Dispatch(const rrr::i32 &target_server_id, const rrr::i32 &req_nr,
                const rrr::i64 &txn_id, const rrr::i64 &expected_time,
                const rrr::i32 &worker_id, const rrr::i32 &num_ops,
                const rrr::i32 &num_involved_shards,
                const std::vector<rrr::i32> &involved_shards,
                const std::string &ops_data, rrr::i32 *req_nr_out,
                rrr::i64 *txn_id_out, rrr::i32 *status,
                rrr::i64 *commit_timestamp, rrr::i32 *num_results,
                std::string *results_data, rrr::DeferredReply *defer);

  void StatusCheck(const rrr::i32 &target_server_id, const rrr::i32 &req_nr,
                   const rrr::i64 &txn_id, rrr::i32 *req_nr_out,
                   rrr::i64 *txn_id_out, rrr::i32 *status,
                   rrr::i64 *commit_timestamp, rrr::i32 *num_results,
                   std::string *results_data,
                   rrr::DeferredReply *defer);

  void OwdPing(const rrr::i32 &target_server_id, const rrr::i32 &req_nr,
               const rrr::i64 &send_time, rrr::i32 *req_nr_out,
               rrr::i32 *status, rrr::DeferredReply *defer);

  void DeadlinePropose(const rrr::i32 &target_server_id, const rrr::i32 &req_nr,
                       const rrr::i64 &tid, const rrr::i64 &proposed_ts,
                       const rrr::i32 &src_shard, const rrr::i32 &phase,
                       rrr::i32 *req_nr_out, rrr::i64 *tid_out,
                       rrr::i64 *proposed_ts_out, rrr::i32 *shard_id,
                       rrr::i32 *status, rrr::DeferredReply *defer);

  void DeadlineConfirm(const rrr::i32 &target_server_id, const rrr::i32 &req_nr,
                       const rrr::i64 &tid, const rrr::i32 &src_shard,
                       const rrr::i64 &new_ts, rrr::i32 *req_nr_out,
                       rrr::i32 *status, rrr::DeferredReply *defer);

  void WatermarkExchange(const rrr::i32 &target_server_id,
                         const rrr::i32 &req_nr, const rrr::i32 &src_shard,
                         const rrr::i32 &num_watermarks,
                         const std::vector<rrr::i64> &watermarks,
                         rrr::i32 *req_nr_out, rrr::i32 *status,
                         rrr::DeferredReply *defer);

protected:
  //===========================================================================
  // Result Storage (for async polling)
  //===========================================================================

  struct TxnResult {
    int status;
    uint64_t commit_timestamp;
    std::vector<std::string> read_results;
    std::chrono::steady_clock::time_point completion_time;
  };

  void StoreResult(uint64_t txn_id, int status, uint64_t commit_ts,
                   const std::vector<std::string> &read_results);

private:
  LuigiServer *server_; // Back-pointer to server

  // Async result storage
  std::unordered_map<uint64_t, TxnResult> completed_txns_;
  mutable std::shared_mutex results_mutex_;
};

} // namespace janus
