#pragma once

#include "__dep__.h"
#include "constants.h"
#include "../rcc/graph.h"
#include "../rcc/graph_marshaler.h"
#include "../command.h"
#include "deptran/procedure.h"
#include "../command_marshaler.h"
#include "../rcc_rpc.h"

class SimpleCommand;
namespace janus {

class TxLogServer;
class RaftServer;
class RaftServiceImpl : public RaftService {
 public:
  RaftServer* sched_;
  RaftServiceImpl(TxLogServer* sched);

  void RequestVote(const ballot_t& candidate_term,
                   const locid_t& candidate_id,
                   const uint64_t& last_log_index,
                   const ballot_t& last_log_term,
                   ballot_t* reply_term,
                   bool_t* vote_granted,
                   rrr::DeferredReply* defer) override;
    
	void AppendEntries(const uint64_t& slot,
                     const uint64_t& leader_term,
                     const uint64_t& leader_prev_log_index,
                     const uint64_t& leader_prev_log_term,
                     const uint64_t& leader_commit_index,
										 const DepId& dep_id,
                     const MarshallDeputy& cmd,
                     uint64_t *follower_append_success,
                     uint64_t *follower_term,
                     uint64_t *follower_last_log_index,
                     rrr::DeferredReply* defer) override;

  void Decide(const uint64_t& slot,
							const DepId& dep_id,
              const MarshallDeputy& cmd,
              rrr::DeferredReply* defer) override;

};

} // namespace janus
