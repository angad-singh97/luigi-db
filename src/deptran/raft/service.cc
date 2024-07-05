
#include "service.h"
#include "server.h"

namespace janus {

RaftServiceImpl::RaftServiceImpl(TxLogServer *sched)
    : sched_((RaftServer*)sched) {
	struct timespec curr_time;
	clock_gettime(CLOCK_MONOTONIC_RAW, &curr_time);
	srand(curr_time.tv_nsec);
}

void RaftServiceImpl::RequestVote(const ballot_t& candidate_term,
                                  const locid_t& candidate_id,
                                  const uint64_t& last_log_index,
                                  const ballot_t& last_log_term,
                                  ballot_t* reply_term,
                                  bool_t* vote_granted,
                                  rrr::DeferredReply* defer) {
  verify(sched_ != nullptr);
  sched_->OnRequestVote(candidate_term,
                        candidate_id,
                        last_log_index,
                        last_log_term,
                        reply_term,
                        vote_granted,
                        std::bind(&rrr::DeferredReply::reply, defer));
}

void RaftServiceImpl::AppendEntries(const uint64_t& slot,
                                        const uint64_t& leader_term,
                                        const uint64_t& leader_prev_log_index,
                                        const uint64_t& leader_prev_log_term,
                                        const uint64_t& leader_commit_index,
                                        const MarshallDeputy& md_cmd,
                                        uint64_t *follower_append_success,
                                        uint64_t *follower_term,
                                        uint64_t *follower_last_log_index,
                                        rrr::DeferredReply* defer) {
  verify(sched_ != nullptr);

  Coroutine::CreateRun([&] () {
    sched_->OnAppendEntries(slot,
                            leader_term,
                            leader_prev_log_index,
                            leader_prev_log_term,
                            leader_commit_index,
                            const_cast<MarshallDeputy&>(md_cmd).sp_data_,
                            follower_append_success,
                            follower_term,
                            follower_last_log_index,
                            std::bind(&rrr::DeferredReply::reply, defer));

  });
	
}

void RaftServiceImpl::Decide(const uint64_t& slot,
                             const MarshallDeputy& md_cmd,
                             rrr::DeferredReply* defer) {
  verify(sched_ != nullptr);
  Coroutine::CreateRun([&] () {
    sched_->OnCommit(slot,
                     const_cast<MarshallDeputy&>(md_cmd).sp_data_);
    defer->reply();
  });
}


} // namespace janus;
