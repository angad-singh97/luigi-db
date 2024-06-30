#pragma once

#include "../__dep__.h"
#include "../constants.h"
#include "../scheduler.h"

namespace janus {


class RaftServer : public TxLogServer {

 public:

  RaftServer();

  RaftServer(Frame *frame);

  ~RaftServer();

  void OnRequestVote(const ballot_t& candinate_term,
                     const uint64_t& candidate_id,
                     const uint64_t& last_log_index,
                     const ballot_t& last_log_term,
                     ballot_t* voter_term,
                     bool_t* vote_granted,
                     const function<void()> &cb);

  void OnAppendEntries(const uint64_t& slot,
                       const ballot_t& leader_term,
                       const uint64_t& leader_prev_log_index,
                       const ballot_t& leader_prev_log_term,
                       const uint64_t& leader_commit_index,
                       const std::shared_ptr<janus::Marshallable> cmd,
                       ballot_t* follower_term,
                       bool_t* follower_append_success,
                       const function<void()> &cb);
  
  void OnDecide(const uint64_t& slot,
                const ballot_t& term,
                const std::shared_ptr<janus::Marshallable> cmd,
                const function<void()> &cb);
};


}