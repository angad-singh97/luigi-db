#pragma once

#include "../__dep__.h"
#include "../communicator.h"
#include "../rcc_rpc.h"

namespace janus {

/**
 * LuigiCommo: Communicator for Luigi protocol
 *
 * All entities connect to all other entities (uses base Communicator logic).
 * RPC targeting happens at the broadcast level:
 * - Coordinator: dispatch/OWD to leaders only
 * - Leaders: deadline propose/confirm to other leaders only
 * - Leaders: watermark exchange to other leaders + coordinator
 * - Leaders: replication to own followers only
 */
class LuigiCommo : public Communicator {
public:
  LuigiCommo() = delete;

  /**
   * Constructor - uses base Communicator to connect to all sites
   */
  explicit LuigiCommo(rusty::Option<rusty::Arc<PollThread>> poll);

  //===========================================================================
  // Send Methods (to specific site)
  //===========================================================================

  shared_ptr<IntEvent>
  SendDispatch(siteid_t site_id, parid_t par_id, rrr::i64 txn_id,
               rrr::i64 expected_time, rrr::i32 worker_id,
               const std::vector<rrr::i32> &involved_shards,
               const std::string &ops_data, rrr::i32 *status,
               rrr::i64 *commit_timestamp, std::string *results_data);

  shared_ptr<IntEvent> SendOwdPing(siteid_t site_id, parid_t par_id,
                                   rrr::i64 send_time, rrr::i32 *status);

  shared_ptr<IntEvent> SendDeadlinePropose(siteid_t site_id, parid_t par_id,
                                           rrr::i64 tid, rrr::i32 src_shard,
                                           rrr::i64 proposed_ts,
                                           rrr::i32 *status);

  shared_ptr<IntEvent> SendDeadlineConfirm(siteid_t site_id, parid_t par_id,
                                           rrr::i64 tid, rrr::i32 src_shard,
                                           rrr::i64 agreed_ts,
                                           rrr::i32 *status);

  shared_ptr<IntEvent>
  SendWatermarkExchange(siteid_t site_id, parid_t par_id, rrr::i32 src_shard,
                        const std::vector<rrr::i64> &watermarks,
                        rrr::i32 *status);

  //===========================================================================
  // Synchronous Convenience Methods
  //===========================================================================

  bool OwdPingSync(parid_t shard_id, rrr::i64 send_time, rrr::i32 *status);

  bool DispatchSync(parid_t shard_id, rrr::i64 txn_id, rrr::i64 expected_time,
                    rrr::i32 worker_id,
                    const std::vector<rrr::i32> &involved_shards,
                    const std::string &ops_data, rrr::i32 *status,
                    rrr::i64 *commit_timestamp, std::string *results_data);

  //===========================================================================
  // Broadcast Helpers (filter to appropriate targets)
  //===========================================================================

  /**
   * Broadcast OWD ping to leaders of specified shards
   * Used by: Coordinator
   */
  void BroadcastOwdPing(int64_t send_time,
                        const std::vector<uint32_t> &shard_ids);

  /**
   * Broadcast deadline proposal to leaders of involved shards (excludes self)
   * Used by: Leaders
   */
  void BroadcastDeadlinePropose(uint64_t tid, int32_t src_shard,
                                int64_t proposed_ts,
                                const std::vector<uint32_t> &involved_shards);

  /**
   * Broadcast deadline confirmation to leaders of involved shards (excludes
   * self) Used by: Leaders
   */
  void BroadcastDeadlineConfirm(uint64_t tid, int32_t src_shard,
                                int64_t agreed_ts,
                                const std::vector<uint32_t> &involved_shards);

  /**
   * Broadcast watermark exchange to other leaders + coordinator
   * Used by: Leaders
   */
  void BroadcastWatermarkExchange(int32_t src_shard,
                                  const std::vector<int64_t> &watermarks,
                                  const std::vector<uint32_t> &involved_shards);

private:
  // On-demand LuigiProxy cache (created from rpc_clients_)
  std::map<siteid_t, LuigiProxy *> luigi_proxies_;

  // Get or create LuigiProxy for a site
  LuigiProxy *GetProxyForSite(siteid_t site_id);
};

} // namespace janus
