
#include "../../rrr/misc/marshal.hpp"
#include "service.h"
#include "server.h"

namespace janus {

// @safe
RaftServiceImpl::RaftServiceImpl(TxLogServer *sched)
    : svr_((RaftServer*)sched) {
	struct timespec curr_time;
	clock_gettime(CLOCK_MONOTONIC_RAW, &curr_time);
	srand(curr_time.tv_nsec);
}

// @safe - Refactored to use lambda instead of std::bind to avoid pointer operations
void RaftServiceImpl::HandleVote(const uint64_t& lst_log_idx,
                                    const ballot_t& lst_log_term,
                                    const siteid_t& can_id,
                                    const ballot_t& can_term,
                                    ballot_t* reply_term,
                                    bool_t *vote_granted,
                                    rrr::DeferredReply* defer) {
  verify(svr_ != nullptr);
  svr_->OnRequestVote(lst_log_idx,lst_log_term, can_id, can_term,
                    reply_term, vote_granted,
                    [defer]() { defer->reply(); });
}

// @safe
void RaftServiceImpl::HandleAppendEntries(const uint64_t& slot,
                                        const ballot_t& ballot,
                                        const uint64_t& leaderCurrentTerm,
                                        const siteid_t& leaderSiteId,
                                        const uint64_t& leaderPrevLogIndex,
                                        const uint64_t& leaderPrevLogTerm,
                                        const uint64_t& leaderCommitIndex,
                                        const MarshallDeputy& md_cmd,
                                        const uint64_t& leaderNextLogTerm,
                                        uint64_t *followerAppendOK,
                                        uint64_t *followerCurrentTerm,
                                        uint64_t *followerLastLogIndex,
                                        rrr::DeferredReply* defer) {
  verify(svr_ != nullptr);

  Coroutine::CreateRun([&] () {
    svr_->OnAppendEntries(slot,
                            ballot,
                            leaderCurrentTerm,
                            leaderSiteId,
                            leaderPrevLogIndex,
                            leaderPrevLogTerm,
                            leaderCommitIndex,
                            const_cast<MarshallDeputy&>(md_cmd).sp_data_,
                            leaderNextLogTerm,
                            followerAppendOK,
                            followerCurrentTerm,
                            followerLastLogIndex,
                            [defer]() { defer->reply(); });

  });
}

// @safe
void RaftServiceImpl::HandleEmptyAppendEntries(const uint64_t& slot,
                                             const ballot_t& ballot,
                                             const uint64_t& leaderCurrentTerm,
                                             const siteid_t& leaderSiteId,
                                             const uint64_t& leaderPrevLogIndex,
                                             const uint64_t& leaderPrevLogTerm,
                                             const uint64_t& leaderCommitIndex,
                                             uint64_t *followerAppendOK,
                                             uint64_t *followerCurrentTerm,
                                             uint64_t *followerLastLogIndex,
                                             rrr::DeferredReply* defer) {
  std::shared_ptr<Marshallable> cmd = nullptr;
  Coroutine::CreateRun([&] () {
    svr_->OnAppendEntries(slot,
                            ballot,
                            leaderCurrentTerm,
                            leaderSiteId,
                            leaderPrevLogIndex,
                            leaderPrevLogTerm,
                            leaderCommitIndex,
                            cmd,
                            0,
                            followerAppendOK,
                            followerCurrentTerm,
                            followerLastLogIndex,
                            [defer]() { defer->reply(); });
  });
}

} // namespace janus;
