/**
 * @file luigi_benchmark_client.cc
 * @brief Implementation of Luigi benchmark client
 */

#include "luigi_benchmark_client.h"

#include <algorithm>
#include <cmath>
#include <future>
#include <iostream>
#include <memory>
#include <numeric>

namespace mako {
namespace luigi {

// Constructor
LuigiBenchmarkClient::LuigiBenchmarkClient(
    const LuigiBenchmarkClient::Config &config)
    : config_(config), luigi_client_(nullptr), transport_(nullptr) {}

LuigiBenchmarkClient::~LuigiBenchmarkClient() {
  // LuigiClient needs to be destroyed before transport
  luigi_client_.reset();

  if (transport_) {
    delete transport_;
    transport_ = nullptr;
  }
}

bool LuigiBenchmarkClient::Initialize() {
  try {
    // 1. Parse configuration
    transport_config_ =
        std::make_unique<transport::Configuration>(config_.config_file);

    // 2. Resolve local URI
    std::string local_uri =
        transport_config_
            ->shard(config_.shard_index, mako::convertCluster(config_.cluster))
            .host;

    // 3. Create FastTransport (mimicking ShardClient logic)
    // Note: nr_req_types=1, physPort=0, numa=0
    transport_ =
        new FastTransport(config_.config_file, local_uri, config_.cluster, 1, 0,
                          0, 0, config_.shard_index, config_.par_id);

    // 4. Create LuigiClient
    luigi_client_ =
        std::make_unique<LuigiClient>(config_.config_file, transport_,
                                      0 // client_id=0 (random/default)
        );

  } catch (const std::exception &e) {
    std::cerr << "Failed to initialize LuigiBenchmarkClient: " << e.what()
              << std::endl;
    return false;
  }

  // Pre-allocate thread stats
  thread_stats_.resize(config_.num_threads);

  return true;
}

mako::luigi::BenchmarkStats
LuigiBenchmarkClient::RunBenchmark(LuigiBenchmarkClient::BenchmarkType type) {
  // Create generator based on type
  switch (type) {
  case BenchmarkType::BM_MICRO:
    generator_ = std::make_unique<MicroTxnGenerator>(config_.gen_config);
    break;
  case BenchmarkType::BM_MICRO_SINGLE:
    generator_ = std::make_unique<SingleShardMicroTxnGenerator>(
        config_.gen_config, config_.shard_index);
    break;
  case BenchmarkType::BM_TPCC:
    generator_ = std::make_unique<TPCCTxnGenerator>(config_.gen_config);
    break;
  }

  if (!generator_) {
    std::cerr << "Failed to create transaction generator" << std::endl;
    return BenchmarkStats{};
  }

  // Clear previous stats
  for (auto &ts : thread_stats_) {
    ts.records.clear();
    ts.committed = 0;
    ts.aborted = 0;
  }

  // Reserve space for latency records (estimate)
  size_t estimated_txns = config_.duration_sec * 10000 / config_.num_threads;
  for (auto &ts : thread_stats_) {
    ts.records.reserve(estimated_txns);
  }

  // Start benchmark
  running_ = true;
  start_time_us_ = GetTimestampUs();
  uint64_t target_end_time = start_time_us_ + config_.duration_sec * 1000000ULL;

  std::cout << "Starting benchmark: type=" << static_cast<int>(type)
            << ", threads=" << config_.num_threads
            << ", duration=" << config_.duration_sec << "s" << std::endl;

  // Launch worker threads
  std::vector<std::thread> workers;
  for (int i = 0; i < config_.num_threads; ++i) {
    workers.emplace_back(&LuigiBenchmarkClient::WorkerThread, this, i);
  }

  // Wait for duration to elapse or Stop() called
  while (running_ && GetTimestampUs() < target_end_time) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  // Signal workers to stop
  running_ = false;
  end_time_us_ = GetTimestampUs();

  // Wait for all workers to finish
  for (auto &w : workers) {
    w.join();
  }

  std::cout << "Benchmark completed" << std::endl;

  // Calculate and return statistics
  return CalculateStats();
}

void LuigiBenchmarkClient::WorkerThread(int thread_id) {
  while (running_) {
    DispatchOneTransaction(thread_id);
  }
}

bool LuigiBenchmarkClient::DispatchOneTransaction(int thread_id) {
  // Generate transaction request
  LuigiTxnRequest req;
  {
    std::lock_guard<std::mutex> lock(generator_mutex_);
    generator_->GetTxnReq(&req, 0, 0);
  }

  // Assign unique transaction ID
  req.txn_id = next_txn_id_.fetch_add(1);

  // Record start time
  TxnRecord record;
  record.txn_id = req.txn_id;
  record.start_time_us = GetTimestampUs();
  record.txn_type = req.txn_type;

  // Dispatch via ShardClient
  bool committed = DispatchRequest(req);

  // Record end time
  record.end_time_us = GetTimestampUs();
  record.committed = committed;

  // Store record
  auto &ts = thread_stats_[thread_id];
  ts.records.push_back(record);
  if (committed) {
    ts.committed++;
  } else {
    ts.aborted++;
  }

  return committed;
}

bool LuigiBenchmarkClient::DispatchRequest(const LuigiTxnRequest &req) {
  if (!luigi_client_) {
    return false;
  }

  // Create promise for synchronous wait
  auto promise = std::make_shared<std::promise<bool>>();
  auto future = promise->get_future();

  // Build dispatch request
  std::map<int, LuigiDispatchBuilder *> requests_per_shard;

  // Group ops by shard (not done here, assuming single shard or simple
  // partitioning for now) For now, we broadcast the full request to all target
  // shards
  // TODO: Ideally allow splitting request by shard

  for (uint32_t shard_id : req.target_shards) {
    auto *builder = new LuigiDispatchBuilder();
    builder->SetTxnId(req.txn_id).SetReqNr(req.req_id);

    // Calculate expected execution time
    uint64_t expected_time =
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
    expected_time += 5000; // +5ms buffer

    builder->SetExpectedTime(expected_time);

    // Add ops
    for (const auto &op : req.ops) {
      if (op.op_type == 1) { // READ
        builder->AddRead(op.table_id, op.key);
      } else { // WRITE
        builder->AddWrite(op.table_id, op.key, op.value);
      }
    }

    requests_per_shard[shard_id] = builder;
  }

  // Call LuigiClient
  luigi_client_->InvokeDispatch(
      req.txn_id, requests_per_shard,
      [promise](char *respBuf) {
        // Success callback
        promise->set_value(true);
      },
      [promise]() {
        // Error callback
        promise->set_value(false);
      },
      250 // timeout ms
  );

  // Wait for result
  if (future.wait_for(std::chrono::milliseconds(1000)) ==
      std::future_status::ready) {
    bool success = future.get();
    // Cleanup builders
    for (auto &pair : requests_per_shard) {
      delete pair.second;
    }
    return success;
  } else {
    // Timeout
    // Cleanup builders
    for (auto &pair : requests_per_shard) {
      delete pair.second;
    }
    return false;
  }
}

void LuigiBenchmarkClient::Stop() { running_ = false; }

mako::luigi::BenchmarkStats LuigiBenchmarkClient::CalculateStats() {
  BenchmarkStats stats;

  // Aggregate all latencies
  std::vector<uint64_t> all_latencies;

  for (const auto &ts : thread_stats_) {
    stats.committed_txns += ts.committed;
    stats.aborted_txns += ts.aborted;

    for (const auto &record : ts.records) {
      uint64_t latency = record.end_time_us - record.start_time_us;
      all_latencies.push_back(latency);
    }
  }

  stats.total_txns = stats.committed_txns + stats.aborted_txns;
  stats.duration_ms = (end_time_us_ - start_time_us_) / 1000;

  // Calculate throughput
  double duration_sec = stats.duration_ms / 1000.0;
  if (duration_sec > 0) {
    stats.throughput_tps = stats.committed_txns / duration_sec;
  }

  // Calculate latency percentiles
  if (!all_latencies.empty()) {
    std::sort(all_latencies.begin(), all_latencies.end());

    // Average
    uint64_t sum =
        std::accumulate(all_latencies.begin(), all_latencies.end(), 0ULL);
    stats.avg_latency_us = static_cast<double>(sum) / all_latencies.size();

    // Percentiles
    auto percentile = [&](double p) -> double {
      size_t idx = static_cast<size_t>(p * all_latencies.size());
      if (idx >= all_latencies.size())
        idx = all_latencies.size() - 1;
      return static_cast<double>(all_latencies[idx]);
    };

    stats.p50_latency_us = percentile(0.50);
    stats.p99_latency_us = percentile(0.99);
    stats.p999_latency_us = percentile(0.999);
  }

  return stats;
}

} // namespace luigi
} // namespace mako
