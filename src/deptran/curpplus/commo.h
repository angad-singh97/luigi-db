#pragma once

#include "../__dep__.h"
#include "../constants.h"
#include "../communicator.h"
#include "../position.h"
#include "server.h"
#include <chrono>
#include <ctime>

namespace janus {

static int maxFailure(int total);

static int fastQuorumSize(int total);

static int quorumSize(int total);

class CurpPlusCoordinatorAcceptQuorumEvent : public QuorumEvent {

 public:
  // using QuorumEvent::QuorumEvent;
  CurpPlusCoordinatorAcceptQuorumEvent(int n_total)
      : QuorumEvent(n_total, quorumSize(n_total)) {
  }

  void FeedResponse(bool y) {
    if (y)
      VoteYes();
    else
      VoteNo();
  }
};

struct AcceptedCmd {
  pair<int, int> cmd_id;
  int last_accepted_status;
  shared_ptr<Marshallable> last_accepted_cmd{nullptr};
  ballot_t last_accepted_ballot;
};

class CurpPlusPrepareQuorumEvent : public QuorumEvent {
  ballot_t max_seen_ballot_ = 0;
  vector<AcceptedCmd> accepted_cmds_;
  int count_ = 0;
  shared_ptr<Marshallable> ready_cmd_{nullptr};
 public:
  // using QuorumEvent::QuorumEvent;
  CurpPlusPrepareQuorumEvent(int n_total)
      : QuorumEvent(n_total, quorumSize(n_total)) {

  }

  void FeedResponse(bool y, ballot_t seen_ballot, int last_accepted_status, MarshallDeputy last_accepted_cmd, ballot_t last_accepted_ballot);
  bool CommitYes();
  bool AcceptYes();
  bool FastAcceptYes();
  bool AcceptAnyYes();
};

class CurpPlusAcceptQuorumEvent : public QuorumEvent {
  ballot_t max_seen_ballot_;
 public:
  // using QuorumEvent::QuorumEvent;
  CurpPlusAcceptQuorumEvent(int n_total)
      : QuorumEvent(n_total, quorumSize(n_total)) {

  }

  void FeedResponse(bool y, ballot_t seen_ballot);
  bool FastYes();
  bool FastNo();
};


  class CurpPlusCommo : public Communicator {
   public:
    CurpPlusCommo() = delete;
    CurpPlusCommo(PollMgr*);

    shared_ptr<IntEvent>
    ForwardResultToCoordinator(parid_t par_id,
                              const shared_ptr<Marshallable>& cmd,
                              Position pos,
                              bool_t accepted);
    
    shared_ptr<CurpPlusCoordinatorAcceptQuorumEvent>
    BroadcastCoordinatorAccept(parid_t par_id,
                              shared_ptr<Position> pos,
                              shared_ptr<Marshallable> cmd);

    shared_ptr<CurpPlusPrepareQuorumEvent>
    BroadcastPrepare(parid_t par_id,
                      shared_ptr<Position> pos,
                      ballot_t ballot);

    shared_ptr<CurpPlusAcceptQuorumEvent>
    BroadcastAccept(parid_t par_id,
                    shared_ptr<Position> pos,
                    shared_ptr<Marshallable> cmd,
                    ballot_t ballot);

    shared_ptr<IntEvent>
    BroadcastCommit(parid_t par_id,
                    shared_ptr<Position> pos,
                    shared_ptr<Marshallable> md_cmd);
    
  };

} // namespace janus
