#include "server.h"
#include "frame.h"
#include "coordinator.h"

namespace janus {

RaftServer::RaftServer() {

}

RaftServer::RaftServer(Frame *frame) {

}

RaftServer::~RaftServer() {

}

void RaftServer::OnRequestVote(const ballot_t& candinate_term,
                               const uint64_t& candidate_id,
                               const uint64_t& last_log_index,
                               const ballot_t& last_log_term,
                               ballot_t* voter_term,
                               bool_t* vote_granted,
                               const function<void()> &cb) {
  
}

void RaftServer::OnAppendEntries(const uint64_t& slot,
                                const ballot_t& leader_term,
                                const uint64_t& leader_prev_log_index,
                                const ballot_t& leader_prev_log_term,
                                const uint64_t& leader_commit_index,
                                const std::shared_ptr<janus::Marshallable> cmd,
                                ballot_t* follower_term,
                                bool_t* follower_append_success,
                                const function<void()> &cb) {
  
}

void RaftServer::OnDecide(const uint64_t& slot,
                          const ballot_t& term,
                          const std::shared_ptr<janus::Marshallable> cmd,
                          const function<void()> &cb) {
  
}

}