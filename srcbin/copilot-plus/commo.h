#pragma once

#include "../__dep__.h"
#include "../communicator.h"
#include "../rcc_rpc.h"
#include "server.h"

namespace janus {

class CopilotPlusSubmitQuorumEvent: public QuorumEvent {
 public:
  struct ResponsePack {
    slotid_t i, j;
    ballot_t ballot;
    bool operator < (const ResponsePack &other) const {
      if (this->i != other.i) return this->i < other.i;
      if (this->j != other.j) return this->j < other.j;
      return this->ballot < other.ballot;
    }
    bool operator == (const ResponsePack &other) const {
      return (this->i == other.i) && (this->j == other.j) && (this->ballot == other.ballot);
    }
  };
 private:
  std::vector<ResponsePack> responses_;
  ResponsePack max_response_;
 public:
  CopilotPlusSubmitQuorumEvent(int n_total, int quorum)
    : QuorumEvent(n_total, quorum) {}
  // TODO: FeedResponse add result?
  void FeedResponse(bool_t accepted, slotid_t i, slotid_t j, ballot_t ballot);
  bool FastYes();
  bool RecoverWithOpYes();
  bool RecoverWithoutOpYes();
  bool IsReady() override;
  //TODO: put in .cc file
  ResponsePack GetMax() {
    return max_response_;
  }
 private:
  //TODO: put in .cc file
  int FindMax(){
    verify(responses_.size() > 0);
    std::sort(responses_.begin(), responses_.end());
    std::vector<ResponsePack>::iterator max_response, last_response;
    int max_len, cur_len;
    for (std::vector<ResponsePack>::iterator it = responses_.begin(); it != responses_.end(); ++it) {
      if (it == responses_.begin()) {
        max_response = it;
        max_len = cur_len = 1;
      } else if (*it == *last_response) {
        if (++cur_len > max_len) {
          max_response = it;
          max_len = cur_len;
        }
      } else {
        max_len = cur_len = 1;
      }
      last_response = it;
    }
    max_response_ = ResponsePack(*max_response);
    return max_len;
  }
  
};

class CopilotPlusFrontRecoverQuorumEvent: public QuorumEvent {
 public:
  CopilotPlusFrontRecoverQuorumEvent(int n_total, int quorum)
    : QuorumEvent(n_total, quorum) {}
  void FeedResponse(bool y) {
    if (y)
      VoteYes();
    else
      VoteNo();
  }
};

class CopilotPlusFrontCommitQuorumEvent: public QuorumEvent {
 public:
  CopilotPlusFrontCommitQuorumEvent(int n_total, int quorum)
    : QuorumEvent(n_total, quorum) {}
  void FeedResponse(bool y) {
    if (y)
      VoteYes();
    else
      VoteNo();
  }
};

class CopilotPlusCommo: public Communicator {
friend class CopilotPlusProxy;
 private:
  CopilotPlusServer *svr_;
 public:
  // void setServer(CopilotPlusServer *svr);
  // CopilotPlusServer* getServer();

 public:
  CopilotPlusCommo() = delete;
  CopilotPlusCommo(PollMgr *poll);

  shared_ptr<CopilotPlusSubmitQuorumEvent>
  BroadcastSubmit(const parid_t par_id,
                  const slotid_t slot_id,
                  const shared_ptr<Marshallable> cmd);

  void
  ForwardReply(const parid_t par_id,
               const slotid_t slot_id,
               const siteid_t site_id,
               const slotid_t i,
               const slotid_t j,
               const ballot_t ballot,
               const bool_t accepted);

  shared_ptr<CopilotPlusFrontRecoverQuorumEvent>
  BroadcastFrontRecover(const parid_t par_id,
                        const slotid_t slot_id,
                        const shared_ptr<Marshallable> cmd,
                        const bool_t commit_no_op_,
                        const slotid_t i,
                        const slotid_t j,
                        const ballot_t ballot);

  shared_ptr<CopilotPlusFrontCommitQuorumEvent>
  BroadcastFrontCommit(const parid_t par_id,
                       const slotid_t slot_id,
                       const shared_ptr<Marshallable> cmd,
                       const bool_t commit_no_op_,
                       const slotid_t i,
                       const slotid_t j,
                       const ballot_t ballot);

};


}