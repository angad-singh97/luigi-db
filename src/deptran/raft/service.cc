
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
                                  const uint64_t& last_log_idx,
                                  const ballot_t& last_log_term,
                                  ballot_t* reply_term,
                                  bool_t* vote_granted,
                                  rrr::DeferredReply* defer) {
  verify(sched_ != nullptr);
  sched_->OnRequestVote(candidate_term,
                        candidate_id,
                        last_log_idx,
                        last_log_term,
                        reply_term,
                        vote_granted,
                        std::bind(&rrr::DeferredReply::reply, defer));
}

void RaftServiceImpl::AppendEntries(const uint64_t& slot,
                                        const ballot_t& ballot,
                                        const uint64_t& leaderCurrentTerm,
                                        const uint64_t& leaderPrevLogIndex,
                                        const uint64_t& leaderPrevLogTerm,
                                        const uint64_t& leaderCommitIndex,
																				const DepId& dep_id,
                                        const MarshallDeputy& md_cmd,
                                        uint64_t *followerAppendOK,
                                        uint64_t *followerCurrentTerm,
                                        uint64_t *followerLastLogIndex,
                                        rrr::DeferredReply* defer) {
  verify(sched_ != nullptr);

  Coroutine::CreateRun([&] () {
    sched_->OnAppendEntries(slot,
                            ballot,
                            leaderCurrentTerm,
                            leaderPrevLogIndex,
                            leaderPrevLogTerm,
                            leaderCommitIndex,
														dep_id,
                            const_cast<MarshallDeputy&>(md_cmd).sp_data_,
                            followerAppendOK,
                            followerCurrentTerm,
                            followerLastLogIndex,
                            std::bind(&rrr::DeferredReply::reply, defer));

  });
	
}

void RaftServiceImpl::Decide(const uint64_t& slot,
                                   const ballot_t& ballot,
																	 const DepId& dep_id,
                                   const MarshallDeputy& md_cmd,
                                   rrr::DeferredReply* defer) {
  verify(sched_ != nullptr);
  Coroutine::CreateRun([&] () {
    sched_->OnCommit(slot,
                     ballot,
                     const_cast<MarshallDeputy&>(md_cmd).sp_data_);
    defer->reply();
  });
}


} // namespace janus;
