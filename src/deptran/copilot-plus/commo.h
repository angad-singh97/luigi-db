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
  int response_received_ = 0;
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
    if (max_len == 3) {
      Log_info("[copilot+] max_response is i=%d j=%d ballot=%d", max_response->i, max_response->j, max_response->ballot);
    }
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
  static int maxFailure(int total);
  static int fastQuorumSize(int total);
  static int quorumSize(int total);
  static int smallQuorumSize(int total);
  // void setServer(CopilotPlusServer *svr);
  // CopilotPlusServer* getServer();

 public:
  CopilotPlusCommo() = delete;
  CopilotPlusCommo(PollMgr *poll);

  shared_ptr<CopilotPlusSubmitQuorumEvent>
  BroadcastSubmit(parid_t par_id,
                  shared_ptr<Marshallable> cmd);

  void
  ForwardReply(parid_t par_id,
               siteid_t site_id,
               slotid_t i,
               slotid_t j,
               ballot_t ballot,
               bool_t accepted);

  shared_ptr<CopilotPlusFrontRecoverQuorumEvent>
  BroadcastFrontRecover(parid_t par_id,
                        shared_ptr<Marshallable> cmd,
                        slotid_t i,
                        slotid_t j,
                        ballot_t ballot);

  shared_ptr<CopilotPlusFrontCommitQuorumEvent>
  BroadcastFrontCommit(parid_t par_id,
                       shared_ptr<Marshallable> cmd,
                       slotid_t i,
                       slotid_t j,
                       ballot_t ballot);

};


}