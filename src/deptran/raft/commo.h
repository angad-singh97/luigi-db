#pragma once

#include "../__dep__.h"
#include "../constants.h"
#include "../communicator.h"

namespace janus {

class TxData;

class RaftVoteQuorumEvent: public QuorumEvent {
 public:
  using QuorumEvent::QuorumEvent;
  bool HasAcceptedValue() {
    return false;
  }
  void FeedResponse(bool y, ballot_t term) {
    if (y) {
      VoteYes();
    } else {
      VoteNo();
      if(term > highest_term_)
      {
        highest_term_ = term ;
      }      
    }
  }
  
  int64_t Term() {
    return highest_term_;
  }
};

class RaftAppendQuorumEvent: public QuorumEvent {
 public:
    uint64_t minIndex;
    using QuorumEvent::QuorumEvent;
    void FeedResponse(bool appendOK, uint64_t index, std::string ip_addr = "") {
        if (appendOK) {
            if ((n_voted_yes_ == 0) && (n_voted_no_ == 0))
                minIndex = index;
            else
                minIndex = std::min(minIndex, index);
            VoteYes();
        } else {
            VoteNo();
        }
        /*Log_debug("fpga-raft comm accept event, "
                  "yes vote: %d, no vote: %d, min index: %d",
                  n_voted_yes_, n_voted_no_, minIndex);*/
    }
};



class RaftCommo : public Communicator {

friend class RaftProxy;
 public:
	std::unordered_map<siteid_t, uint64_t> matchedIndex {};
	int index;
	
  RaftCommo() = delete;
  RaftCommo(PollMgr*);
  shared_ptr<RaftVoteQuorumEvent>
  BroadcastVote(parid_t par_id,
                        slotid_t lst_log_idx,
                        ballot_t lst_log_term,
                        parid_t self_id,
                        ballot_t cur_term ); 
  shared_ptr<RaftAppendQuorumEvent>
  BroadcastAppendEntries(parid_t par_id,
                         siteid_t leader_site_id,
                         slotid_t slot_id,
                         i64 dep_id,
                         ballot_t ballot,
                         bool isLeader,
                         uint64_t currentTerm,
                         uint64_t prevLogIndex,
                         uint64_t prevLogTerm,
                         uint64_t commitIndex,
                         shared_ptr<Marshallable> cmd);
  void BroadcastDecide(const parid_t par_id,
                       const slotid_t slot_id,
											 const i64 dep_id,
                       const ballot_t ballot,
                       const shared_ptr<Marshallable> cmd);
};

} // namespace janus

