#pragma once

#include "__dep__.h"
#include "constants.h"
#include "../marshallable.h"

#include "../rcc/graph.h"
#include "../rcc/graph_marshaler.h"
#include "../command.h"
#include "deptran/procedure.h"
#include "../command_marshaler.h"

#include "../rcc_rpc.h"

namespace janus {


class TxLogServer;
class RaftServer;
class RaftServiceImpl : public RaftService {

 public:

  RaftServer* sched_;

  RaftServiceImpl(TxLogServer* sched);

  void RequestVote(const ballot_t& candinate_term,
                   const uint64_t& candidate_id,
                   const uint64_t& last_log_index,
                   const ballot_t& last_log_term,
                   ballot_t* voter_term,
                   bool_t* vote_granted,
                   rrr::DeferredReply* defer);
  
  void AppendEntries(const uint64_t& slot,
                     const ballot_t& leader_term,
                     const uint64_t& leader_prev_log_index,
                     const ballot_t& leader_prev_log_term,
                     const uint64_t& leader_commit_index,
                     const MarshallDeputy& md_cmd,
                     ballot_t* follower_term,
                     bool_t* follower_append_success,
                     rrr::DeferredReply* defer);
  
  void Decide(const uint64_t& slot,
              const ballot_t& term,
              const MarshallDeputy& cmd,
              rrr::DeferredReply* defer);

};

}