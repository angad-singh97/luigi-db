#pragma once

#include "../__dep__.h"
#include "../communicator.h"
#include "../constants.h"
#include "../rcc_rpc.h"

namespace janus {

/**
 * Role of this Luigi entity in the system.
 */
enum class LuigiRole {
  COORDINATOR, // Benchmark client - connects to all leaders
  LEADER,      // Shard leader - connects to other leaders + own followers
  FOLLOWER     // Shard replica - connects to own leader only
};

/**
 * LuigiCommo: Role-aware Communicator for Luigi protocol
 *
 * Connection topology:
 * - Coordinator: connects to all shard leaders
 * - Leader: connects to other leaders + own followers
 * - Follower: connects to own leader only
 */
class LuigiCommo : public Communicator {
public:
  LuigiCommo() = delete;

  /**
   * Constructor for coordinator role (connects to all leaders)
   */
  explicit LuigiCommo(rusty::Option<rusty::Arc<PollThread>> poll);

  /**
   * Constructor for server role (leader or follower)
   */
  LuigiCommo(LuigiRole role, uint32_t shard_id, siteid_t site_id,
             rusty::Option<rusty::Arc<PollThread>> poll = rusty::None);

  LuigiRole GetRole() const { return role_; }
  uint32_t GetShardId() const { return shard_id_; }
  siteid_t GetSiteId() const { return site_id_; }

  /**
   * Send Dispatch RPC to a specific server
   */
  shared_ptr<IntEvent>
  SendDispatch(siteid_t site_id, parid_t par_id, rrr::i64 txn_id,
               rrr::i64 expected_time, rrr::i32 worker_id,
               const std::vector<rrr::i32> &involved_shards,
               const std::string &ops_data, rrr::i32 *status,
               rrr::i64 *commit_timestamp, std::string *results_data);

  /**
   * Send OWD Ping RPC
   */
  shared_ptr<IntEvent> SendOwdPing(siteid_t site_id, parid_t par_id,
                                   rrr::i64 send_time, rrr::i32 *status);

  /**
   * Send DeadlinePropose RPC (phase 1)
   */
  shared_ptr<IntEvent> SendDeadlinePropose(siteid_t site_id, parid_t par_id,
                                           rrr::i64 tid, rrr::i32 src_shard,
                                           rrr::i64 proposed_ts,
                                           rrr::i32 *status);

  /**
   * Send DeadlineConfirm RPC (phase 2)
   */
  shared_ptr<IntEvent> SendDeadlineConfirm(siteid_t site_id, parid_t par_id,
                                           rrr::i64 tid, rrr::i32 src_shard,
                                           rrr::i64 agreed_ts,
                                           rrr::i32 *status);

  /**
   * Send WatermarkExchange RPC
   */
  shared_ptr<IntEvent>
  SendWatermarkExchange(siteid_t site_id, parid_t par_id, rrr::i32 src_shard,
                        const std::vector<rrr::i64> &watermarks,
                        rrr::i32 *status);

  //===========================================================================
  // Synchronous Convenience Methods (blocking)
  //===========================================================================

  /**
   * Sync OWD ping to a shard leader. Blocks until response.
   * @return true on success
   */
  bool OwdPingSync(parid_t shard_id, rrr::i64 send_time, rrr::i32 *status);

  /**
   * Sync dispatch to a shard leader. Blocks until response.
   * @return true on success
   */
  bool DispatchSync(parid_t shard_id, rrr::i64 txn_id, rrr::i64 expected_time,
                    rrr::i32 worker_id,
                    const std::vector<rrr::i32> &involved_shards,
                    const std::string &ops_data, rrr::i32 *status,
                    rrr::i64 *commit_timestamp, std::string *results_data);

  //===========================================================================
  // Broadcast Helpers
  //===========================================================================
  void BroadcastDeadlinePropose(uint64_t tid, int32_t src_shard,
                                int64_t proposed_ts,
                                const std::vector<uint32_t> &involved_shards);

  void BroadcastOwdPing(int64_t send_time,
                        const std::vector<uint32_t> &involved_shards);

  void BroadcastDeadlineConfirm(uint64_t tid, int32_t src_shard,
                                int64_t agreed_ts,
                                const std::vector<uint32_t> &involved_shards);

  void BroadcastWatermarkExchange(int32_t src_shard,
                                  const std::vector<int64_t> &watermarks,
                                  const std::vector<uint32_t> &involved_shards);

private:
  /**
   * Setup connections based on role.
   */
  void ConnectByRole();

  /**
   * Connect to a specific site and create LuigiProxy.
   */
  std::pair<int, LuigiProxy *> ConnectToLuigiSite(Config::SiteInfo &site);

  LuigiRole role_ = LuigiRole::COORDINATOR;
  uint32_t shard_id_ = 0;
  siteid_t site_id_ = 0;

  // Site-to-LuigiProxy mapping for role-aware connections
  std::map<siteid_t, LuigiProxy *> luigi_proxies_;
};

} // namespace janus
