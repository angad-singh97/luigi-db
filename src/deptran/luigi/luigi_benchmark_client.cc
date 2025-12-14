/**
 * @file luigi_benchmark_client.cc
 * @brief Implementation of Luigi benchmark client
 */

#include "luigi_benchmark_client.h"
#include "mako/lib/shardClient.h"

#include <algorithm>
#include <iostream>
#include <numeric>
#include <cmath>

namespace mako {
namespace luigi {

LuigiBenchmarkClient::LuigiBenchmarkClient(const Config& config)
    : config_(config), shard_client_(nullptr) {
}

LuigiBenchmarkClient::~LuigiBenchmarkClient() {
    if (shard_client_) {
        auto* client = static_cast<ShardClient*>(shard_client_);
        client->stop();
        delete client;
        shard_client_ = nullptr;
    }
}

bool LuigiBenchmarkClient::Initialize() {
    // Create ShardClient
    try {
        auto* client = new ShardClient(
            config_.config_file,
            config_.cluster,
            config_.shard_index,
            config_.par_id);
        shard_client_ = client;
    } catch (const std::exception& e) {
        std::cerr << "Failed to create ShardClient: " << e.what() << std::endl;
        return false;
    }
    
    // Pre-allocate thread stats
    thread_stats_.resize(config_.num_threads);
    
    return true;
}

BenchmarkStats LuigiBenchmarkClient::RunBenchmark(BenchmarkType type) {
    // Create generator based on type
    switch (type) {
        case BenchmarkType::MICRO:
            generator_ = std::make_unique<MicroTxnGenerator>(config_.gen_config);
            break;
        case BenchmarkType::MICRO_SINGLE_SHARD:
            generator_ = std::make_unique<SingleShardMicroTxnGenerator>(
                config_.gen_config, config_.shard_index);
            break;
        case BenchmarkType::TPCC:
            generator_ = std::make_unique<TPCCTxnGenerator>(config_.gen_config);
            break;
    }
    
    if (!generator_) {
        std::cerr << "Failed to create transaction generator" << std::endl;
        return BenchmarkStats{};
    }
    
    // Clear previous stats
    for (auto& ts : thread_stats_) {
        ts.records.clear();
        ts.committed = 0;
        ts.aborted = 0;
    }
    
    // Reserve space for latency records (estimate)
    size_t estimated_txns = config_.duration_sec * 10000 / config_.num_threads;
    for (auto& ts : thread_stats_) {
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
    for (auto& w : workers) {
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
        req = generator_->GetTxnReq();
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
    auto& ts = thread_stats_[thread_id];
    ts.records.push_back(record);
    if (committed) {
        ts.committed++;
    } else {
        ts.aborted++;
    }
    
    return committed;
}

bool LuigiBenchmarkClient::DispatchRequest(const LuigiTxnRequest& req) {
    if (!shard_client_) {
        return false;
    }
    
    auto* client = static_cast<ShardClient*>(shard_client_);
    
    // Convert LuigiTxnRequest to format expected by remoteLuigiDispatch
    std::vector<int> table_ids;
    std::vector<uint8_t> op_types;
    std::vector<std::string> keys;
    std::vector<std::string> values;
    
    // Convert ops
    for (const auto& op : req.ops) {
        table_ids.push_back(op.table_id);
        op_types.push_back(op.op_type);
        keys.push_back(op.key);
        values.push_back(op.value);
    }
    
    // Calculate expected execution time
    // Use current time + estimated OWD as expected_time
    uint64_t expected_time = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    // Add buffer for network delay (e.g., 5ms)
    expected_time += 5000;
    
    // Output parameters
    std::map<int, uint64_t> execute_timestamps;
    std::map<int, std::vector<std::string>> read_results;
    
    // Dispatch
    int result = client->remoteLuigiDispatch(
        req.txn_id,
        expected_time,
        table_ids,
        op_types,
        keys,
        values,
        execute_timestamps,
        read_results);
    
    // Check result
    return (result == 0);  // MakoErrorCode::OK = 0
}

void LuigiBenchmarkClient::Stop() {
    running_ = false;
}

BenchmarkStats LuigiBenchmarkClient::CalculateStats() {
    BenchmarkStats stats;
    
    // Aggregate all latencies
    std::vector<uint64_t> all_latencies;
    
    for (const auto& ts : thread_stats_) {
        stats.committed_txns += ts.committed;
        stats.aborted_txns += ts.aborted;
        
        for (const auto& record : ts.records) {
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
        uint64_t sum = std::accumulate(all_latencies.begin(), all_latencies.end(), 0ULL);
        stats.avg_latency_us = static_cast<double>(sum) / all_latencies.size();
        
        // Percentiles
        auto percentile = [&](double p) -> double {
            size_t idx = static_cast<size_t>(p * all_latencies.size());
            if (idx >= all_latencies.size()) idx = all_latencies.size() - 1;
            return static_cast<double>(all_latencies[idx]);
        };
        
        stats.p50_latency_us = percentile(0.50);
        stats.p99_latency_us = percentile(0.99);
        stats.p999_latency_us = percentile(0.999);
    }
    
    return stats;
}

}  // namespace luigi
}  // namespace mako
