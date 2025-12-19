#include "owd.h"
#include "client.h"
#include "commo.h"
#include "../__dep__.h"

#include <algorithm>
#include <iostream>

namespace janus {
namespace luigi {

LuigiOWD::LuigiOWD() {}

LuigiOWD::~LuigiOWD() { stop(); }

LuigiOWD &LuigiOWD::getInstance() {
  static LuigiOWD instance;
  return instance;
}

void LuigiOWD::init(std::shared_ptr<LuigiCommo> commo, int local_shard_idx,
                    int num_shards) {
  if (initialized_.load()) {
    return; // Already initialized
  }

  commo_ = commo;
  local_shard_idx_ = local_shard_idx;
  num_shards_ = num_shards;

  // Initialize OWD table with default values
  {
    std::lock_guard<std::mutex> lock(owd_mutex_);
    for (int i = 0; i < num_shards_; i++) {
      if (i == local_shard_idx_) {
        owd_table_[i] = 0; // Local shard has zero OWD
      } else {
        owd_table_[i] = DEFAULT_INITIAL_OWD_MS;
      }
    }
  }

  // Create per-shard clients for OWD pings
  for (int i = 0; i < num_shards_; i++) {
    if (i != local_shard_idx_) {
      auto client = std::make_unique<LuigiClient>(i);
      client->Initialize(commo_);
      shard_clients_[i] = std::move(client);
    }
  }

  initialized_.store(true);

  Log_info("[LuigiOWD] Initialized for %d shards (local=%d)", num_shards_,
           local_shard_idx_);
}

void LuigiOWD::start() {
  if (!initialized_.load()) {
    Log_error("[LuigiOWD] Cannot start - not initialized");
    return;
  }

  if (running_.load()) {
    return; // Already running
  }

  running_.store(true);

  // Start background ping thread
  ping_thread_ = std::thread(&LuigiOWD::pingLoop, this);

  Log_info("[LuigiOWD] Started background ping thread");
}

void LuigiOWD::stop() {
  if (!running_.load()) {
    return;
  }

  running_.store(false);

  if (ping_thread_.joinable()) {
    ping_thread_.join();
  }

  Log_info("[LuigiOWD] Stopped");
}

uint64_t LuigiOWD::getOWD(int shard_idx) const {
  std::lock_guard<std::mutex> lock(owd_mutex_);
  auto it = owd_table_.find(shard_idx);
  if (it != owd_table_.end()) {
    return it->second;
  }
  return DEFAULT_INITIAL_OWD_MS;
}

uint64_t LuigiOWD::getMaxOWD(const std::vector<int> &shard_indices) const {
  uint64_t max_owd = 0;
  std::lock_guard<std::mutex> lock(owd_mutex_);
  for (int idx : shard_indices) {
    auto it = owd_table_.find(idx);
    if (it != owd_table_.end()) {
      max_owd = std::max(max_owd, it->second);
    } else {
      max_owd = std::max(max_owd, DEFAULT_INITIAL_OWD_MS);
    }
  }
  return max_owd;
}

uint64_t LuigiOWD::getExpectedTimestamp(
    const std::vector<int> &involved_shards) const {
  uint64_t max_owd = getMaxOWD(involved_shards);
  return getCurrentTimeMillis() + max_owd + HEADROOM_MS;
}

uint64_t LuigiOWD::getExpectedTimestamp(uint64_t shard_bitmask) const {
  std::vector<int> involved_shards;
  for (int i = 0; i < 64 && i < num_shards_; i++) {
    if (shard_bitmask & (1ULL << i)) {
      involved_shards.push_back(i);
    }
  }
  return getExpectedTimestamp(involved_shards);
}

void LuigiOWD::updateOWD(int shard_idx, uint64_t rtt_ms) {
  // Estimate OWD as RTT/2
  uint64_t owd = rtt_ms / 2;

  std::lock_guard<std::mutex> lock(owd_mutex_);
  // Exponential moving average with alpha=0.3
  auto it = owd_table_.find(shard_idx);
  if (it != owd_table_.end()) {
    it->second = static_cast<uint64_t>(0.7 * it->second + 0.3 * owd);
  } else {
    owd_table_[shard_idx] = owd;
  }
}

std::vector<int> LuigiOWD::getRemoteShards() const {
  std::vector<int> remote;
  for (int i = 0; i < num_shards_; i++) {
    if (i != local_shard_idx_) {
      remote.push_back(i);
    }
  }
  return remote;
}

void LuigiOWD::pingLoop() {
  while (running_.load()) {
    // Ping all remote shards
    for (int shard_idx : getRemoteShards()) {
      if (!running_.load())
        break;
      pingShard(shard_idx);
    }

    // Sleep before next round
    std::this_thread::sleep_for(std::chrono::milliseconds(PING_INTERVAL_MS));
  }
}

void LuigiOWD::pingShard(int shard_idx) {
  auto it = shard_clients_.find(shard_idx);
  if (it == shard_clients_.end()) {
    return;
  }

  uint64_t send_time = getCurrentTimeMillis();
  int status;

  bool ok = it->second->OwdPing(send_time, &status);

  if (ok) {
    uint64_t recv_time = getCurrentTimeMillis();
    uint64_t rtt = recv_time - send_time;
    updateOWD(shard_idx, rtt);
  }
}

} // namespace luigi
} // namespace janus
