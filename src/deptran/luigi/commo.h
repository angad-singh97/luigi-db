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
      siteid_t site_id, parid_t par_id, rrr::i32 target_server_id,
      rrr::i32 req_nr, rrr::i64 txn_id, rrr::i64 expected_time,
      rrr::i32 worker_id, rrr::i32 num_ops, rrr::i32 num_involved_shards,
      const std::vector<rrr::i32> &involved_shards, const std::string &ops_data,
      rrr::i32 *req_nr_out, rrr::i64 *txn_id_out, rrr::i32 *status,
      rrr::i64 *commit_timestamp, rrr::i32 *num_results,
      std::string *results_data);

  /**
   * Send StatusCheck RPC to a specific server
   */
  shared_ptr<IntEvent>
  SendStatusCheck(siteid_t site_id, parid_t par_id, rrr::i32 target_server_id,
                  rrr::i32 req_nr, rrr::i64 txn_id, rrr::i32 *req_nr_out,
                  rrr::i64 *txn_id_out, rrr::i32 *status,
                  rrr::i64 *commit_timestamp, rrr::i32 *num_results,
                  std::string *results_data);

  /**
   * Send OWD Ping RPC
   */
  shared_ptr<IntEvent> SendOwdPing(siteid_t site_id, parid_t par_id,
                                   rrr::i32 target_server_id, rrr::i32 req_nr,
                                   rrr::i64 send_time, rrr::i32 *req_nr_out,
                                   rrr::i32 *status);

  /**
   * Send DeadlinePropose RPC
   */
  shared_ptr<IntEvent> SendDeadlinePropose(
      siteid_t site_id, parid_t par_id, rrr::i32 target_server_id,
      rrr::i32 req_nr, rrr::i64 tid, rrr::i64 proposed_ts, rrr::i32 src_shard,
      rrr::i32 phase, rrr::i32 *req_nr_out, rrr::i64 *tid_out,
      rrr::i64 *proposed_ts_out, rrr::i32 *shard_id, rrr::i32 *status);

  /**
   * Send DeadlineConfirm RPC
   */
  shared_ptr<IntEvent> SendDeadlineConfirm(siteid_t site_id, parid_t par_id,
                                           rrr::i32 target_server_id,
                                           rrr::i32 req_nr, rrr::i64 tid,
                                           rrr::i32 src_shard, rrr::i64 new_ts,
                                           rrr::i32 *req_nr_out,
                                           rrr::i32 *status);

  /**
   * Send WatermarkExchange RPC
   */
  shared_ptr<IntEvent>
  SendWatermarkExchange(siteid_t site_id, parid_t par_id,
                        rrr::i32 target_server_id, rrr::i32 req_nr,
                        rrr::i32 src_shard, rrr::i32 num_watermarks,
                        const std::vector<rrr::i64> &watermarks,
                        rrr::i32 *req_nr_out, rrr::i32 *status);
};

} // namespace janus
