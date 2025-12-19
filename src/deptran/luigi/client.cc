/**
 * LuigiClient: RRR-based client implementation for Luigi protocol.
 */

#include "client.h"
#include "../config.h"

#include <chrono>

namespace janus {

LuigiClient::LuigiClient(parid_t partition_id) : partition_id_(partition_id) {}

LuigiClient::~LuigiClient() {}

void LuigiClient::Initialize(std::shared_ptr<LuigiCommo> commo) {
  commo_ = commo;
}

siteid_t LuigiClient::GetLeaderSiteId() {
  // Get the leader site for this partition from config
  auto config = Config::GetConfig();
  auto sites = config->SitesByPartitionId(partition_id_);
  if (sites.empty()) {
    Log_error("No sites found for partition %d", partition_id_);
    return 0;
  }
  // Return first site (leader) for now
  // TODO: support leader election
  return sites[0].id;
}

std::string LuigiClient::SerializeOps(const std::vector<LuigiOp> &ops) {
  std::string data;
  for (const auto &op : ops) {
    // Format: table_id (2 bytes) | op_type (1 byte) | key_len (2 bytes) | 
    // value_len (2 bytes) | key | value
    uint16_t table_id = op.table_id;
    data.append(reinterpret_cast<const char *>(&table_id), sizeof(table_id));
    data.push_back(static_cast<char>(op.op_type));
    uint16_t key_len = op.key.size();
    data.append(reinterpret_cast<const char *>(&key_len), sizeof(key_len));
    uint16_t value_len = op.value.size();
    data.append(reinterpret_cast<const char *>(&value_len), sizeof(value_len));
    data.append(op.key);
    data.append(op.value);
  }
  return data;
}

std::vector<std::string>
LuigiClient::DeserializeResults(const std::string &data, int num_results) {
  std::vector<std::string> results;
  size_t offset = 0;
  for (int i = 0; i < num_results && offset < data.size(); i++) {
    if (offset + sizeof(uint16_t) > data.size())
      break;
    uint16_t len;
    memcpy(&len, data.data() + offset, sizeof(len));
    offset += sizeof(len);
    if (offset + len > data.size())
      break;
    results.emplace_back(data.data() + offset, len);
    offset += len;
  }
  return results;
}

bool LuigiClient::Dispatch(uint64_t txn_id, uint64_t expected_time,
                           const std::vector<LuigiOp> &ops,
                           const std::vector<uint32_t> &involved_shards,
                           uint32_t worker_id, int *status_out,
                           uint64_t *commit_ts_out,
                           std::vector<std::string> *results_out) {
  if (!commo_) {
    Log_error("LuigiClient not initialized");
    return false;
  }

  siteid_t site_id = GetLeaderSiteId();

  // Convert involved_shards to rrr::i32 vector
  std::vector<rrr::i32> involved_shards_i32(involved_shards.begin(),
                                            involved_shards.end());

  // Serialize operations
  std::string ops_data = SerializeOps(ops);

  // Output variables
  rrr::i32 status;
  rrr::i64 commit_timestamp;
  std::string results_data;

  // Send RPC and wait for response
  auto ev = commo_->SendDispatch(
      site_id, partition_id_,
      txn_id, expected_time, worker_id,
      involved_shards_i32, ops_data,
      &status, &commit_timestamp, &results_data);

  // Wait for completion
  ev->Wait();

  // Parse results
  *status_out = status;
  *commit_ts_out = commit_timestamp;
  // Count results from results_data (simplified - just return as single result)
  if (!results_data.empty()) {
    results_out->push_back(results_data);
  }

  return status == 0; // LUIGI_SUCCESS
}

void LuigiClient::DispatchAsync(uint64_t txn_id, uint64_t expected_time,
                                const std::vector<LuigiOp> &ops,
                                const std::vector<uint32_t> &involved_shards,
                                uint32_t worker_id,
                                LuigiResponseCallback callback) {
  if (!commo_) {
    Log_error("LuigiClient not initialized");
    callback(-1, 0, {});
    return;
  }

  siteid_t site_id = GetLeaderSiteId();

  std::vector<rrr::i32> involved_shards_i32(involved_shards.begin(),
                                            involved_shards.end());
  std::string ops_data = SerializeOps(ops);

  // Allocate output variables on heap for async callback
  auto status = std::make_shared<rrr::i32>();
  auto commit_timestamp = std::make_shared<rrr::i64>();
  auto results_data = std::make_shared<std::string>();

  auto ev = commo_->SendDispatch(
      site_id, partition_id_,
      txn_id, expected_time, worker_id,
      involved_shards_i32, ops_data,
      status.get(), commit_timestamp.get(),
      results_data.get());

  // Set up callback when event completes
  std::thread([=]() {
    ev->Wait();
    std::vector<std::string> results;
    if (!results_data->empty()) {
      results.push_back(*results_data);
    }
    callback(*status, *commit_timestamp, results);
  }).detach();
}

bool LuigiClient::OwdPing(uint64_t send_time, int *status_out) {
  if (!commo_) {
    Log_error("LuigiClient not initialized");
    return false;
  }

  siteid_t site_id = GetLeaderSiteId();

  rrr::i32 status;

  auto ev = commo_->SendOwdPing(site_id, partition_id_, send_time, &status);

  ev->Wait();

  *status_out = status;
  return status == 0;
}

bool LuigiClient::DeadlinePropose(uint64_t tid, uint32_t src_shard,
                                  uint64_t proposed_ts,
                                  int *status_out) {
  if (!commo_) {
    Log_error("LuigiClient not initialized");
    return false;
  }

  siteid_t site_id = GetLeaderSiteId();

  rrr::i32 status;

  auto ev = commo_->SendDeadlinePropose(site_id, partition_id_,
                                        tid, src_shard, proposed_ts,
                                        &status);

  ev->Wait();

  *status_out = status;
  return status == 0;
}

bool LuigiClient::DeadlineConfirm(uint64_t tid, uint32_t src_shard,
                                  uint64_t agreed_ts, int *status_out) {
  if (!commo_) {
    Log_error("LuigiClient not initialized");
    return false;
  }

  siteid_t site_id = GetLeaderSiteId();

  rrr::i32 status;

  auto ev = commo_->SendDeadlineConfirm(site_id, partition_id_,
                                        tid, src_shard, agreed_ts, &status);

  ev->Wait();

  *status_out = status;
  return status == 0;
}

bool LuigiClient::WatermarkExchange(uint32_t src_shard,
                                    const std::vector<uint64_t> &watermarks,
                                    int *status_out) {
  if (!commo_) {
    Log_error("LuigiClient not initialized");
    return false;
  }

  siteid_t site_id = GetLeaderSiteId();

  std::vector<rrr::i64> watermarks_i64(watermarks.begin(), watermarks.end());

  rrr::i32 status;

  auto ev = commo_->SendWatermarkExchange(site_id, partition_id_,
                                          src_shard, watermarks_i64, &status);

  ev->Wait();

  *status_out = status;
  return status == 0;
}

//=============================================================================
// Async methods for scheduler (fire-and-forget with callbacks)
//=============================================================================

void LuigiClient::InvokeDeadlinePropose(uint32_t target_partition, uint64_t tid,
                                        uint32_t src_shard, uint64_t proposed_ts,
                                        std::function<void(int status)> response_cb,
                                        std::function<void()> error_cb) {
  if (!commo_) {
    Log_error("LuigiClient not initialized");
    if (error_cb) error_cb();
    return;
  }

  // Get site for target partition from config
  auto config = Config::GetConfig();
  auto sites = config->SitesByPartitionId(target_partition);
  if (sites.empty()) {
    Log_warn("No sites for partition %u", target_partition);
    if (error_cb) error_cb();
    return;
  }
  siteid_t site_id = sites[0].id;  // Use first site (leader)

  // Allocate outputs on heap for async callback
  auto status = std::make_shared<rrr::i32>();

  auto ev = commo_->SendDeadlinePropose(site_id, target_partition,
                                        tid, src_shard, proposed_ts,
                                        status.get());

  // Fire-and-forget: just log if needed, don't wait
  Log_debug("InvokeDeadlinePropose sent to partition %u, tid=%lu",
            target_partition, tid);
}

void LuigiClient::InvokeDeadlineConfirm(uint32_t target_partition, uint64_t tid,
                                        uint64_t agreed_ts,
                                        std::function<void(int status)> response_cb,
                                        std::function<void()> error_cb) {
  if (!commo_) {
    Log_error("LuigiClient not initialized");
    if (error_cb) error_cb();
    return;
  }

  // Get site for target partition from config
  auto config = Config::GetConfig();
  auto sites = config->SitesByPartitionId(target_partition);
  if (sites.empty()) {
    Log_warn("No sites for partition %u", target_partition);
    if (error_cb) error_cb();
    return;
  }
  siteid_t site_id = sites[0].id;  // Use first site (leader)

  // Allocate outputs on heap for async callback
  auto status = std::make_shared<rrr::i32>();

  auto ev = commo_->SendDeadlineConfirm(site_id, target_partition,
                                        tid, partition_id_, agreed_ts,
                                        status.get());

  Log_debug("InvokeDeadlineConfirm sent to partition %u, tid=%lu",
            target_partition, tid);
}

void LuigiClient::InvokeWatermarkExchange(
    uint32_t target_partition, const std::vector<uint64_t> &watermarks,
    std::function<void(char *)> response_cb, std::function<void()> error_cb) {
  if (!commo_) {
    Log_error("LuigiClient not initialized");
    if (error_cb) error_cb();
    return;
  }

  // Get site for target partition from config
  auto config = Config::GetConfig();
  auto sites = config->SitesByPartitionId(target_partition);
  if (sites.empty()) {
    Log_warn("No sites for partition %u", target_partition);
    if (error_cb) error_cb();
    return;
  }
  siteid_t site_id = sites[0].id;  // Use first site (leader)

  std::vector<rrr::i64> watermarks_i64(watermarks.begin(), watermarks.end());

  // Allocate outputs on heap for async callback
  auto status = std::make_shared<rrr::i32>();

  auto ev = commo_->SendWatermarkExchange(site_id, target_partition,
                                          partition_id_, watermarks_i64,
                                          status.get());

  Log_debug("InvokeWatermarkExchange sent to partition %u", target_partition);
}

} // namespace janus
