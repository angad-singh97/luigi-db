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
  RaftServer* svr_;
  RaftServiceImpl(TxLogServer* sched);

  // void RequestVote(const ballot_t& candidate_term,
  //                  const locid_t& candidate_id,
  //                  const uint64_t& last_log_index,
  //                  const ballot_t& last_log_term,
  //                  ballot_t* reply_term,
  //                  bool_t* vote_granted,
  //                  rrr::DeferredReply* defer) override;
    
	// void AppendEntries(const uint64_t& slot,
  //                    const uint64_t& leader_term,
  //                    const uint64_t& leader_prev_log_index,
  //                    const uint64_t& leader_prev_log_term,
  //                    const MarshallDeputy& cmd,
  //                    const uint64_t& leader_commit_index,
  //                    uint64_t *follower_term,
  //                    uint64_t *follower_append_success,
  //                    uint64_t *follower_last_log_index,
  //                    rrr::DeferredReply* defer) override;

  // void Decide(const uint64_t& slot,
  //             const MarshallDeputy& cmd,
  //             rrr::DeferredReply* defer) override;

  void RequestVote(const uint64_t& leaderTerm,
                   const uint64_t& candidateId,
                   const uint64_t& lastLogIndex,
                   const uint64_t& lastLogTerm,
                   uint64_t* receiverTerm,
                   bool_t* voteGranted,
                   rrr::DeferredReply* defer) override;

  void AppendEntries(const uint64_t& leaderTerm,
                     const uint64_t& leaderId,
                     const uint64_t& prevLogIndex,
                     const uint64_t& prevLogTerm,
                     const MarshallDeputy& entry,
                     const uint64_t& entryTerm,
                     const uint64_t& leaderCommit,
                     uint64_t* receiverTerm,
                     bool_t* success,
                     rrr::DeferredReply* defer) override;

  void EmptyAppendEntries(const uint64_t& leaderTerm,
                          const uint64_t& leaderId,
                          const uint64_t& prevLogIndex,
                          const uint64_t& prevLogTerm,
                          const uint64_t& leaderCommit,
                          uint64_t* receiverTerm,
                          bool_t* success,
                          rrr::DeferredReply* defer) override;

  void HelloRpc(const std::string& req,
                std::string* res,
                rrr::DeferredReply* defer) override;

};

} // namespace janus
