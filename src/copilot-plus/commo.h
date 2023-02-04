#pragma once

#include "../__dep__.h"
#include "../communicator.h"
#include "../rcc_rpc.h"

namespace janus {

class CopilotPlusSubmitQuorumEvent: public QuorumEvent {
 private:
  struct ResponsePack {
    slotid_t i, j;
    ballot_t ballot;
    bool operator < (const ResponsePack &other) const {
      if (this->i != other.i) return this->i < other.i;
      if (this->j != other.j) return this->j < other.j;
      return this->ballot < other.ballot;
    }
  };
  std::vector<ResponsePack> responses_;
 public:
  CopilotPlusSubmitQuorumEvent(int n_total, int quorum)
    : QuorumEvent(n_total, quorum) {}
  // TODO: FeedResponse add result?
  void FeedResponse(slotid_t i, slotid_t j, ballot_t ballot);
  bool FastYes();
  bool FastNo();
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
 public:
  static int maxFailure(int total);
  static int fastQuorumSize(int total);
  static int quorumSize(int total);

 public:
  CopilotPlusCommo() = delete;
  CopilotPlusCommo(PollMgr *poll);

  shared_ptr<CopilotPlusSubmitQuorumEvent>
  BroadcastSubmit(parid_t par_id,
                  shared_ptr<Marshallable> cmd);

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