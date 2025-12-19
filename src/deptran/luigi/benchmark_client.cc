/**
 * @file benchmark_client.cc
 * @brief Implementation of Luigi benchmark client using RRR framework
 */

#include "benchmark_client.h"
#include "owd.h"

#include <algorithm>
#include <future>
#include <memory>
#include <numeric>

namespace janus {
namespace luigi {

LuigiBenchmarkClient::LuigiBenchmarkClient(const Config &config)
    : config_(config) {}

LuigiBenchmarkClient::~LuigiBenchmarkClient() {
  Stop();
  clients_.clear();
}

bool LuigiBenchmarkClient::Initialize(std::shared_ptr<LuigiCommo> commo) {
  commo_ = commo;

  if (!commo_) {
    Log_error("LuigiBenchmarkClient: commo is null");
    return false;
  }

  // Create clients for each partition/shard
  for (int i = 0; i < config_.num_shards; i++) {
    auto client = std::make_unique<LuigiClient>(i);
    client->Initialize(commo_);
    clients_[i] = std::move(client);
  }

  // Initialize thread stats
  thread_stats_.resize(config_.num_threads);

  Log_info("LuigiBenchmarkClient initialized with %d shards, %d threads",
           config_.num_shards, config_.num_threads);

  return true;
}

void LuigiBenchmarkClient::Stop() {
  running_.store(false);
}

BenchmarkStats LuigiBenchmarkClient::RunBenchmark(BenchmarkType type) {
  current_benchmark_type_ = type;

  // Create appropriate generator
  {
    std::lock_guard<std::mutex> lock(generator_mutex_);
    switch (type) {
    case BenchmarkType::BM_MICRO:
      generator_ = std::make_unique<MicroTxnGenerator>(config_.gen_config);
      break;
    case BenchmarkType::BM_MICRO_SINGLE:
      generator_ = std::make_unique<MicroTxnGenerator>(config_.gen_config);
      // Force single-shard by setting shard_num to 1 in generator
      break;
    case BenchmarkType::BM_TPCC:
      generator_ = std::make_unique<TPCCTxnGenerator>(config_.gen_config);
      break;
    }
  }

  // Reset stats
  for (auto &ts : thread_stats_) {
    ts.records.clear();
    ts.committed = 0;
    ts.aborted = 0;
  }
  next_txn_id_.store(1);

  // Start benchmark
  running_.store(true);
  start_time_us_ = GetTimestampUs();

  // Spawn worker threads
  std::vector<std::thread> workers;
  for (int i = 0; i < config_.num_threads; i++) {
    workers.emplace_back(&LuigiBenchmarkClient::WorkerThread, this, i);
  }

  // Wait for duration
  std::this_thread::sleep_for(std::chrono::seconds(config_.duration_sec));

  // Stop and wait for workers
  running_.store(false);
  for (auto &w : workers) {
    w.join();
  }

  end_time_us_ = GetTimestampUs();

  return CalculateStats();
}

void LuigiBenchmarkClient::WorkerThread(int thread_id) {
  while (running_.load()) {
    if (!DispatchOneTransaction(thread_id)) {
      // Small sleep on failure to avoid tight loop
      std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
  }
}

bool LuigiBenchmarkClient::DispatchOneTransaction(int thread_id) {
  LuigiTxnRequest req;

  // Generate transaction
  {
    std::lock_guard<std::mutex> lock(generator_mutex_);
    if (!generator_) {
      return false;
    }
    uint64_t local_id = next_txn_id_.fetch_add(1);
    uint64_t txn_id = (static_cast<uint64_t>(thread_id) << 48) | local_id;
    generator_->GetTxnReq(&req, static_cast<uint32_t>(txn_id), thread_id);
    req.txn_id = txn_id;
  }

  // Calculate worker ID
  uint32_t worker_id = config_.worker_id_base + thread_id;

  // Record start time
  uint64_t start_us = GetTimestampUs();

  // Dispatch
  bool committed = DispatchRequest(req, worker_id);

  // Record end time
  uint64_t end_us = GetTimestampUs();

  // Record result
  TxnRecord record;
  record.txn_id = req.txn_id;
  record.start_time_us = start_us;
  record.end_time_us = end_us;
  record.committed = committed;
  record.txn_type = req.txn_type;

  thread_stats_[thread_id].records.push_back(record);
  if (committed) {
    thread_stats_[thread_id].committed++;
  } else {
    thread_stats_[thread_id].aborted++;
  }

  return true;
}

bool LuigiBenchmarkClient::DispatchRequest(const LuigiTxnRequest &req,
                                           uint32_t worker_id) {
  // Determine involved shards
  std::vector<uint32_t> involved_shards;
  for (const auto &op : req.ops) {
    // Simple hash-based sharding
    uint32_t shard = 0;
    if (config_.num_shards > 1) {
      size_t hash = std::hash<std::string>{}(op.key);
      shard = hash % config_.num_shards;
    }
    if (std::find(involved_shards.begin(), involved_shards.end(), shard) ==
        involved_shards.end()) {
      involved_shards.push_back(shard);
    }
  }

  // For single-shard benchmark, force all to local shard
  if (current_benchmark_type_ == BenchmarkType::BM_MICRO_SINGLE) {
    involved_shards = {static_cast<uint32_t>(config_.shard_index)};
  }

  // Calculate expected timestamp using OWD
  std::vector<int> shard_indices(involved_shards.begin(), involved_shards.end());
  uint64_t expected_time =
      LuigiOWD::getInstance().getExpectedTimestamp(shard_indices);

  // Send to primary shard (first involved shard)
  parid_t primary_shard = involved_shards.empty() ? 0 : involved_shards[0];

  auto it = clients_.find(primary_shard);
  if (it == clients_.end()) {
    Log_error("No client for shard %d", primary_shard);
    return false;
  }

  int status;
  uint64_t commit_ts;
  std::vector<std::string> results;

  bool ok = it->second->Dispatch(req.txn_id, expected_time, req.ops,
                                 involved_shards, worker_id, &status,
                                 &commit_ts, &results);

  return ok && status == 0; // LUIGI_SUCCESS
}

BenchmarkStats LuigiBenchmarkClient::CalculateStats() {
  BenchmarkStats stats;

  // Aggregate all records
  std::vector<uint64_t> latencies;
  for (const auto &ts : thread_stats_) {
    stats.committed_txns += ts.committed;
    stats.aborted_txns += ts.aborted;
    for (const auto &rec : ts.records) {
      latencies.push_back(rec.end_time_us - rec.start_time_us);
    }
  }

  stats.total_txns = stats.committed_txns + stats.aborted_txns;
  stats.duration_ms = (end_time_us_ - start_time_us_) / 1000;

  if (stats.duration_ms > 0) {
    stats.throughput_tps =
        1000.0 * stats.committed_txns / stats.duration_ms;
  }

  if (!latencies.empty()) {
    // Sort for percentiles
    std::sort(latencies.begin(), latencies.end());

    // Average
    double sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);
    stats.avg_latency_us = sum / latencies.size();

    // Percentiles
    size_t n = latencies.size();
    stats.p50_latency_us = latencies[n * 50 / 100];
    stats.p99_latency_us = latencies[n * 99 / 100];
    stats.p999_latency_us = latencies[std::min(n - 1, n * 999 / 1000)];
  }

  return stats;
}

} // namespace luigi
} // namespace janus
