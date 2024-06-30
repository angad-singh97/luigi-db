#include "service.h"
#include "server.h"

namespace janus {

RaftServiceImpl::RaftServiceImpl(TxLogServer* sched)
  : sched_((RaftServer*)sched) {

}

void RaftServiceImpl::RequestVote(const ballot_t& candinate_term,
                                  const uint64_t& candidate_id,
                                  const uint64_t& last_log_index,
                                  const ballot_t& last_log_term,
                                  ballot_t* voter_term,
                                  bool_t* vote_granted,
                                  rrr::DeferredReply* defer) {
  verify(sched_ != nullptr);
  sched_->OnRequestVote(candinate_term,
                        candidate_id,
                        last_log_index,
                        last_log_term,
                        voter_term,
                        vote_granted,
                        std::bind(&rrr::DeferredReply::reply, defer));
}

void RaftServiceImpl::AppendEntries(const uint64_t& slot,
                                    const ballot_t& leader_term,
                                    const uint64_t& leader_prev_log_index,
                                    const ballot_t& leader_prev_log_term,
                                    const uint64_t& leader_commit_index,
                                    const MarshallDeputy& md_cmd,
                                    ballot_t* follower_term,
                                    bool_t* follower_append_success,
                                    rrr::DeferredReply* defer) {
  verify(sched_ != nullptr);
  sched_->OnAppendEntries(slot,
                          leader_term,
                          leader_prev_log_index,
                          leader_prev_log_term,
                          leader_commit_index,
                          md_cmd.sp_data_,
                          follower_term,
                          follower_append_success,
                          std::bind(&rrr::DeferredReply::reply, defer));
}

void RaftServiceImpl::Decide(const uint64_t& slot,
                             const ballot_t& term,
                             const MarshallDeputy& md_cmd,
                             rrr::DeferredReply* defer) {
  verify(sched_ != nullptr);
  sched_->OnDecide(slot,
                   term,
                   md_cmd.sp_data_,
                   std::bind(&rrr::DeferredReply::reply, defer));
}


}