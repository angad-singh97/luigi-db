#pragma once

/**
 * LuigiClient: Client for Luigi timestamp-ordered execution protocol.
 *
 * Uses RRR framework for network communication via LuigiCommo.
 */

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "../__dep__.h"
#include "commo.h"
#include "luigi_common.h"
#include "luigi_entry.h"

namespace janus {

/**
 * Response/error continuation types.
 */
using LuigiResponseCallback = std::function<void(
    int status, uint64_t commit_ts, const std::vector<std::string> &results)>;
using LuigiErrorCallback = std::function<void(int error_code)>;

/**
 * LuigiClient: Client for Luigi protocol operations.
 *
 * Uses RRR framework (LuigiCommo) for network communication.
 */
class LuigiClient {
public:
  LuigiClient(parid_t partition_id);
  ~LuigiClient();

  /**
   * Initialize the client with a communicator.
   * @param commo Shared pointer to LuigiCommo (manages RPC connections)
   */
  void Initialize(std::shared_ptr<LuigiCommo> commo);

  /**
   * Set the partition this client targets.
   */
  void SetPartitionId(parid_t par_id) { partition_id_ = par_id; }

  //=========================================================================
  // Luigi Protocol Operations (Synchronous)
  //=========================================================================

  /**
   * Send a Luigi dispatch request (synchronous).
   *
   * @param txn_id Transaction ID
   * @param expected_time Expected execution timestamp
   * @param ops Operations to execute
   * @param involved_shards All shards involved in this transaction
   * @param worker_id Worker ID for per-worker replication
   * @param status_out Output: status code
   * @param commit_ts_out Output: commit timestamp
   * @param results_out Output: read results
   * @return true on success
   */
  bool Dispatch(uint64_t txn_id, uint64_t expected_time,
                const std::vector<LuigiOp> &ops,
                const std::vector<uint32_t> &involved_shards,
                uint32_t worker_id, int *status_out, uint64_t *commit_ts_out,
                std::vector<std::string> *results_out);

  /**
   * Send a Luigi dispatch request (asynchronous).
   *
   * @param txn_id Transaction ID
   * @param expected_time Expected execution timestamp  
   * @param ops Operations to execute
   * @param involved_shards All shards involved
   * @param worker_id Worker ID
   * @param callback Callback with (status, commit_ts, results)
   */
  void DispatchAsync(uint64_t txn_id, uint64_t expected_time,
                     const std::vector<LuigiOp> &ops,
                     const std::vector<uint32_t> &involved_shards,
                     uint32_t worker_id, LuigiResponseCallback callback);

  /**
   * Send OWD (one-way delay) ping to measure latency.
   *
   * @param send_time Timestamp when ping was sent
   * @param status_out Output: status code
   * @return true on success
   */
  bool OwdPing(uint64_t send_time, int *status_out);

  /**
   * Broadcast deadline proposal to a shard (phase 1).
   *
   * @param tid Transaction ID
   * @param src_shard Source shard ID
   * @param proposed_ts This shard's proposed timestamp
   * @param status_out Output: status
   * @return true on success
   */
  bool DeadlinePropose(uint64_t tid, uint32_t src_shard, uint64_t proposed_ts,
                       int *status_out);

  /**
   * Confirm agreed timestamp after computing max (phase 2).
   *
   * @param tid Transaction ID
   * @param src_shard Source shard ID
   * @param agreed_ts Agreed max timestamp
   * @param status_out Output: status
   * @return true on success
   */
  bool DeadlineConfirm(uint64_t tid, uint32_t src_shard, uint64_t agreed_ts,
                       int *status_out);

  /**
   * Exchange watermarks with remote shard.
   *
   * @param src_shard Source shard ID
   * @param watermarks Watermark values
   * @param status_out Output: status
   * @return true on success
   */
  bool WatermarkExchange(uint32_t src_shard,
                         const std::vector<uint64_t> &watermarks,
                         int *status_out);

  //=========================================================================
  // Luigi Protocol Operations (Asynchronous with target partition)
  // These methods send RPCs to a specified remote partition (fire-and-forget)
  //=========================================================================

  /**
   * Broadcast deadline proposal to a specific remote partition (async, phase 1).
   *
   * @param target_partition Target partition ID
   * @param tid Transaction ID
   * @param src_shard Source shard ID (this shard)
   * @param proposed_ts This shard's proposed timestamp
   * @param response_cb Called on response
   * @param error_cb Called on error
   */
  void InvokeDeadlinePropose(uint32_t target_partition, uint64_t tid,
                             uint32_t src_shard, uint64_t proposed_ts,
                             std::function<void(int status)> response_cb,
                             std::function<void()> error_cb);

  /**
   * Send deadline confirmation to a specific remote partition (async, phase 2).
   *
   * @param target_partition Target partition ID
   * @param tid Transaction ID
   * @param agreed_ts Agreed max timestamp
   * @param response_cb Called on response
   * @param error_cb Called on error
   */
  void InvokeDeadlineConfirm(uint32_t target_partition, uint64_t tid,
                             uint64_t agreed_ts,
                             std::function<void(int status)> response_cb,
                             std::function<void()> error_cb);

  /**
   * Send watermark exchange to a specific remote partition (async).
   *
   * @param target_partition Target partition ID
   * @param watermarks Watermark values
   * @param response_cb Called on response
   * @param error_cb Called on error
   */
  void InvokeWatermarkExchange(uint32_t target_partition,
                               const std::vector<uint64_t> &watermarks,
                               std::function<void(char *)> response_cb,
                               std::function<void()> error_cb);

private:
  // Serialize operations to wire format
  std::string SerializeOps(const std::vector<LuigiOp> &ops);
  
  // Deserialize results from wire format
  std::vector<std::string> DeserializeResults(const std::string &data,
                                               int num_results);

  // Get site ID for this partition (picks leader)
  siteid_t GetLeaderSiteId();

  parid_t partition_id_;
  std::shared_ptr<LuigiCommo> commo_;
};

} // namespace janus
