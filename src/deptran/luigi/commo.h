#pragma once

#include "../__dep__.h"
#include "../communicator.h"
#include "../constants.h"
#include "luigi.h"

namespace janus {

/**
 * LuigiCommo: Communicator for Luigi protocol
 *
 * Follows the same pattern as RaftCommo - inherits from Communicator
 * and uses LuigiProxy for RPC calls.
 */
class LuigiCommo : public Communicator {
public:
  LuigiCommo() = delete;
  LuigiCommo(rusty::Option<rusty::Arc<PollThread>> poll = rusty::None);

  /**
   * Send Dispatch RPC to a specific server
   */
  shared_ptr<IntEvent> SendDispatch(
      siteid_t site_id, parid_t par_id,
      rrr::i64 txn_id, rrr::i64 expected_time,
      rrr::i32 worker_id,
      const std::vector<rrr::i32> &involved_shards, const std::string &ops_data,
      rrr::i32 *status,
      rrr::i64 *commit_timestamp,
      std::string *results_data);

  /**
   * Send OWD Ping RPC
   */
  shared_ptr<IntEvent> SendOwdPing(siteid_t site_id, parid_t par_id,
                                   rrr::i64 send_time,
                                   rrr::i32 *status);

  /**
   * Send DeadlinePropose RPC (phase 1 - broadcast our timestamp)
   */
  shared_ptr<IntEvent> SendDeadlinePropose(
      siteid_t site_id, parid_t par_id,
      rrr::i64 tid, rrr::i32 src_shard, rrr::i64 proposed_ts,
      rrr::i32 *status);

  /**
   * Send DeadlineConfirm RPC (phase 2 - confirm agreed timestamp)
   */
  shared_ptr<IntEvent> SendDeadlineConfirm(siteid_t site_id, parid_t par_id,
                                           rrr::i64 tid,
                                           rrr::i32 src_shard, rrr::i64 agreed_ts,
                                           rrr::i32 *status);

  /**
   * Send WatermarkExchange RPC
   */
  shared_ptr<IntEvent>
  SendWatermarkExchange(siteid_t site_id, parid_t par_id,
                        rrr::i32 src_shard,
                        const std::vector<rrr::i64> &watermarks,
                        rrr::i32 *status);

  /**
   * Broadcast DeadlinePropose to all involved shard leaders.
   * For each involved_shard, looks up leader site_id and sends proposal.
   */
  void BroadcastDeadlinePropose(
      uint64_t tid, int32_t src_shard, int64_t proposed_ts,
      const std::vector<uint32_t>& involved_shards);

  /**
   * Broadcast OwdPing to all involved shard leaders.
   */
  void BroadcastOwdPing(
      int64_t send_time, const std::vector<uint32_t>& involved_shards);

  /**
   * Broadcast DeadlineConfirm to all involved shard leaders.
   */
  void BroadcastDeadlineConfirm(
      uint64_t tid, int32_t src_shard, int64_t agreed_ts,
      const std::vector<uint32_t>& involved_shards);

  /**
   * Broadcast WatermarkExchange to all involved shard leaders.
   */
  void BroadcastWatermarkExchange(
      int32_t src_shard, const std::vector<int64_t>& watermarks,
      const std::vector<uint32_t>& involved_shards);
};

} // namespace janus
