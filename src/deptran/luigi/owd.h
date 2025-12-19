#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace janus {
class LuigiClient;
class LuigiCommo;
} // namespace janus

namespace janus {
namespace luigi {

/**
 * One-Way Delay (OWD) measurement service for Luigi protocol.
 *
 * Runs a background thread that periodically pings all remote shards
 * and maintains a table of estimated one-way delays.
 * Used to calculate expected_timestamp for transactions.
 *
 * Uses RRR framework for communication.
 */
class LuigiOWD {
public:
  // Headroom added to max OWD when calculating expected timestamp (ms)
  static constexpr uint64_t HEADROOM_MS = 10;

  // Default initial OWD estimate (ms) - used before first measurement
  static constexpr uint64_t DEFAULT_INITIAL_OWD_MS = 50;

  // How often to ping remote shards to update OWD (ms)
  static constexpr uint64_t PING_INTERVAL_MS = 100;

  // Get singleton instance
  static LuigiOWD &getInstance();

  /**
   * Initialize the service.
   *
   * @param commo Shared pointer to LuigiCommo for RPC communication
   * @param local_shard_idx This shard's index
   * @param num_shards Total number of shards
   */
  void init(std::shared_ptr<LuigiCommo> commo, int local_shard_idx,
            int num_shards);

  // Start the background ping thread
  void start();

  // Stop the background ping thread
  void stop();

  // Check if initialized
  bool isInitialized() const { return initialized_.load(); }

  // Check if running
  bool isRunning() const { return running_.load(); }

  // Get the current one-way delay estimate for a specific shard (ms)
  uint64_t getOWD(int shard_idx) const;

  // Get the maximum OWD among the specified shards (ms)
  uint64_t getMaxOWD(const std::vector<int> &shard_indices) const;

  // Calculate expected timestamp for a transaction involving the given shards
  // Returns: current_time_ms + max_owd + HEADROOM_MS
  uint64_t getExpectedTimestamp(const std::vector<int> &involved_shards) const;

  // Calculate expected timestamp given a bitmask of involved shards
  // Bit i is set if shard i is involved
  uint64_t getExpectedTimestamp(uint64_t shard_bitmask) const;

  // Update OWD for a shard based on measured RTT (called internally)
  // rtt_ms is the round-trip time; OWD is estimated as RTT/2
  void updateOWD(int shard_idx, uint64_t rtt_ms);

  // Get list of remote shards (excluding local)
  std::vector<int> getRemoteShards() const;

  // Get number of shards
  int getNumShards() const { return num_shards_; }

  // Get local shard index
  int getLocalShardIdx() const { return local_shard_idx_; }

private:
  LuigiOWD();
  ~LuigiOWD();

  // Disable copy/move
  LuigiOWD(const LuigiOWD &) = delete;
  LuigiOWD &operator=(const LuigiOWD &) = delete;

  // Background thread function
  void pingLoop();

  // Ping a single shard and update OWD
  void pingShard(int shard_idx);

  int num_shards_ = 0;
  int local_shard_idx_ = 0;

  // RRR-based communication
  std::shared_ptr<LuigiCommo> commo_;

  // Per-shard clients for OWD pings
  std::map<int, std::unique_ptr<LuigiClient>> shard_clients_;

  // OWD estimates per shard (in milliseconds)
  mutable std::mutex owd_mutex_;
  std::map<int, uint64_t> owd_table_;

  // Background thread control
  std::atomic<bool> initialized_{false};
  std::atomic<bool> running_{false};
  std::thread ping_thread_;
};

// Helper function to get current time in milliseconds
inline uint64_t getCurrentTimeMillis() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

} // namespace luigi
} // namespace janus
