
#include "service.h"
#include "server.h"

namespace janus {

RaftServiceImpl::RaftServiceImpl(TxLogServer *sched)
    : svr_((RaftServer*)sched) {
	struct timespec curr_time;
	clock_gettime(CLOCK_MONOTONIC_RAW, &curr_time);
	srand(curr_time.tv_nsec);
}

// void RaftServiceImpl::RequestVote(const ballot_t& candidate_term,
//                                   const locid_t& candidate_id,
//                                   const uint64_t& last_log_index,
//                                   const ballot_t& last_log_term,
//                                   ballot_t* reply_term,
//                                   bool_t* vote_granted,
//                                   rrr::DeferredReply* defer) {
//   verify(sched_ != nullptr);
//   sched_->OnRequestVote(candidate_term,
//                         candidate_id,
//                         last_log_index,
//                         last_log_term,
//                         reply_term,
//                         vote_granted,
//                         std::bind(&rrr::DeferredReply::reply, defer));
// }

// void RaftServiceImpl::AppendEntries(const uint64_t& slot,
//                                         const uint64_t& leader_term,
//                                         const uint64_t& leader_prev_log_index,
//                                         const uint64_t& leader_prev_log_term,
//                                         const MarshallDeputy& md_cmd,
//                                         const uint64_t& leader_commit_index,
//                                         uint64_t *follower_term,
//                                         uint64_t *follower_append_success,
//                                         uint64_t *follower_last_log_index,
//                                         rrr::DeferredReply* defer) {
//   verify(sched_ != nullptr);

//   Coroutine::CreateRun([&] () {
//     sched_->OnAppendEntries(slot,
//                             leader_term,
//                             leader_prev_log_index,
//                             leader_prev_log_term,
//                             const_cast<MarshallDeputy&>(md_cmd).sp_data_,
//                             leader_commit_index,
//                             follower_term,
//                             follower_append_success,
//                             follower_last_log_index,
//                             std::bind(&rrr::DeferredReply::reply, defer));

//   });
	
// }

// void RaftServiceImpl::Decide(const uint64_t& slot,
//                              const MarshallDeputy& md_cmd,
//                              rrr::DeferredReply* defer) {
//   verify(sched_ != nullptr);
//   Coroutine::CreateRun([&] () {
//     sched_->OnCommit(slot,
//                      const_cast<MarshallDeputy&>(md_cmd).sp_data_);
//     defer->reply();
//   });
// }

void RaftServiceImpl::RequestVote(
  const uint64_t& leaderTerm,
  const uint64_t& candidateId,
  const uint64_t& lastLogIndex,
  const uint64_t& lastLogTerm,
  uint64_t* receiverTerm,
  bool_t* voteGranted,
  rrr::DeferredReply* defer
) {
    // Log_info("Received HandleRequestVote");
    {
      std::lock_guard<std::recursive_mutex> lk(svr_->mtx_);

      *voteGranted = false;
      if (leaderTerm > svr_->currentTerm) {
        svr_->currentTerm = leaderTerm;
        svr_->identity = svr_->IS_FOLLOWER;
        svr_->votedFor = svr_->VOTED_FOR_NULL;
      }

      if (
        leaderTerm >= svr_->currentTerm &&
        (svr_->votedFor == candidateId || svr_->votedFor == svr_->VOTED_FOR_NULL) &&
        (
          lastLogTerm > svr_->logQueue.getLastLogTerm() ||
          (
            lastLogTerm == svr_->logQueue.getLastLogTerm() &&
            lastLogIndex >= svr_->logQueue.getLastLogIndex()
          )
        )
      ) {
        *voteGranted = true;
        svr_->votedFor = candidateId;
      }
      
      *receiverTerm = svr_->currentTerm;
    }

  defer->reply();
}

void RaftServiceImpl::AppendEntries(
  const uint64_t& leaderTerm,
  const uint64_t& leaderId,
  const uint64_t& prevLogIndex,
  const uint64_t& prevLogTerm,
  const MarshallDeputy& entry,
  const uint64_t& entryTerm,
  const uint64_t& leaderCommit,
  uint64_t* receiverTerm,
  bool_t* success,
  rrr::DeferredReply* defer
) {
  {
  std::lock_guard<std::recursive_mutex> lk(svr_->mtx_);
    *success = false;

    if (leaderTerm > svr_->currentTerm) {
      svr_->currentTerm = leaderTerm;
      svr_->identity = svr_->IS_FOLLOWER;
      svr_->votedFor = svr_->VOTED_FOR_NULL;
    }

    if (leaderTerm < svr_->currentTerm) {
      // Log_info("[+] Receiving out-of-date AppendEntries from %d", leaderId);
    } else if (
      !(svr_->logQueue.getLastLogIndex() >= prevLogIndex &&
        svr_->logQueue.getLogTerm(prevLogIndex) == prevLogTerm)
    ) {
      svr_->heatbeatReceived = true;

      // Log_info("[+] from %d | fail there", leaderId);
    } else {
      *success = true;
      svr_->heatbeatReceived = true;

      uint64_t thisLogIndex = prevLogIndex + 1;

      if (svr_->logQueue.getLastLogIndex() >= thisLogIndex &&
          svr_->logQueue.getLogTerm(thisLogIndex) != leaderTerm) {
            svr_->logQueue.deleteLogs(thisLogIndex);
          }
      
      if (svr_->logQueue.getLastLogIndex() < thisLogIndex) {
        std::shared_ptr<Marshallable> cmd = const_cast<MarshallDeputy&>(entry).sp_data_;

        // Log_info("[+] %d adding index %d |Term %d| into Log", svr_->getThisServerID(), prevLogIndex + 1, entryTerm);

        verify(cmd != nullptr);
        svr_->logQueue.addLog(cmd, entryTerm);
      }

      if (leaderCommit > svr_->commitIndex) {
        svr_->commitIndex = min(leaderCommit, svr_->logQueue.getLastLogIndex());
        svr_->commitLogs();
      }
    }

    *receiverTerm = svr_->currentTerm;
  }
  defer->reply();
}

void RaftServiceImpl::EmptyAppendEntries(
  const uint64_t& leaderTerm,
  const uint64_t& leaderId,
  const uint64_t& prevLogIndex,
  const uint64_t& prevLogTerm,
  const uint64_t& leaderCommit,
  uint64_t* receiverTerm,
  bool_t* success,
  rrr::DeferredReply* defer
) {
  {
  std::lock_guard<std::recursive_mutex> lk(svr_->mtx_);
  *success = false;

  // Log_info("[+] Receive heartbeat | %d <- %d", leaderId, svr_->getThisServerID());

  if (leaderTerm > svr_->currentTerm) {
    svr_->currentTerm = leaderTerm;
    svr_->identity = svr_->IS_FOLLOWER;
    svr_->votedFor = svr_->VOTED_FOR_NULL;
  }

  if (leaderTerm < svr_->currentTerm) {
    // Log_info("[+] Receiving out-of-date AppendEntries from %d", leaderId);
  } else {
    *success = (svr_->logQueue.getLastLogIndex() >= prevLogIndex &&
                svr_->logQueue.getLogTerm(prevLogIndex) == prevLogTerm);
    svr_->heatbeatReceived = true;

    if (*success) {
      if (leaderCommit > svr_->commitIndex) {
        // if (svr_->commitIndex != min(leaderCommit, svr_->logQueue.getLastLogIndex())) {
        //   Log_info("[+] ID %d update commitIndex [%d -> %d] in HeartBeat | pIdx: %d | pTerm: %d",
        //    svr_->getThisServerID(), svr_->commitIndex, min(leaderCommit, svr_->logQueue.getLastLogIndex()), prevLogIndex, prevLogTerm);
        // }

        svr_->commitIndex = min(leaderCommit, svr_->logQueue.getLastLogIndex());
        svr_->commitLogs();
      } 
    } else if (svr_->logQueue.getLastLogIndex() >= prevLogIndex) {
      svr_->logQueue.deleteLogs(prevLogIndex);
    }
  }

  *receiverTerm = svr_->currentTerm;
  }
  defer->reply();
}

void RaftServiceImpl::HelloRpc(const string& req,
                                     string* res,
                                     rrr::DeferredReply* defer) {
  /* Your code here */
  Log_info("receive an rpc: %s", req.c_str());
  *res = "world";
  defer->reply();
}

} // namespace janus;
