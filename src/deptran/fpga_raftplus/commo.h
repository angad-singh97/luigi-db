#pragma once

#include "../__dep__.h"
#include "../constants.h"
#include "../communicator.h"

namespace janus {

class TxData;

class FpgaRaftPlusForwardQuorumEvent: public QuorumEvent {
 public:
  using QuorumEvent::QuorumEvent;
  uint64_t CommitIdx()
  {
    return cmt_idx_ ;
  }
  void FeedResponse(uint64_t cmt_idx) {
    VoteYes();
    cmt_idx_ = cmt_idx ;
  }
};

class FpgaRaftPlusPrepareQuorumEvent: public QuorumEvent {
 public:
  using QuorumEvent::QuorumEvent;
//  ballot_t max_ballot_{0};
  bool HasAcceptedValue() {
    // TODO implement this
    return false;
  }
  void FeedResponse(bool y) {
    if (y) {
      VoteYes();
    } else {
      VoteNo();
    }
  }
};

class FpgaRaftPlusVoteQuorumEvent: public QuorumEvent {
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

class FpgaRaftPlusVote2FPGAQuorumEvent: public QuorumEvent {
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

class FpgaRaftPlusAcceptQuorumEvent: public QuorumEvent {
 public:
  using QuorumEvent::QuorumEvent;
  void FeedResponse(bool y) {
    if (y) {
      VoteYes();
    } else {
      VoteNo();
    }
    /*Log_debug("multi-paxos comm accept event, "
              "yes vote: %d, no vote: %d",
              n_voted_yes_, n_voted_no_);*/
  }
};

class FpgaRaftPlusAppendQuorumEvent: public QuorumEvent {
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



class FpgaRaftPlusCommo : public Communicator {

friend class FpgaRaftPlusProxy;
 public:
	std::unordered_map<siteid_t, uint64_t> matchedIndex {};
	int index;
	
  FpgaRaftPlusCommo() = delete;
  FpgaRaftPlusCommo(PollMgr*);
  shared_ptr<FpgaRaftPlusForwardQuorumEvent>
  SendForward(parid_t par_id, parid_t self_id, shared_ptr<Marshallable> cmd);  
	void BroadcastHeartbeat(parid_t par_id,
													uint64_t logIndex);
	void SendHeartbeat(parid_t par_id,
										 siteid_t site_id,
										 uint64_t logIndex);
	//ONLY FOR SIMULATION
  void SendAppendEntriesAgain(siteid_t site_id,
															parid_t par_id,
															slotid_t slot_id,
															ballot_t ballot,
															bool isLeader,
															uint64_t currentTerm,
															uint64_t prevLogIndex,
															uint64_t prevLogTerm,
															uint64_t commitIndex,
															shared_ptr<Marshallable> cmd);
  shared_ptr<FpgaRaftPlusPrepareQuorumEvent>
  BroadcastPrepare(parid_t par_id,
                   slotid_t slot_id,
                   ballot_t ballot);
  void BroadcastPrepare(parid_t par_id,
                        slotid_t slot_id,
                        ballot_t ballot,
                        const function<void(Future *fu)> &callback);
  shared_ptr<FpgaRaftPlusVoteQuorumEvent>
  BroadcastVote(parid_t par_id,
                        slotid_t lst_log_idx,
                        ballot_t lst_log_term,
                        parid_t self_id,
                        ballot_t cur_term );
  void BroadcastVote(parid_t par_id,
                        slotid_t lst_log_idx,
                        ballot_t lst_log_term,
                        parid_t self_id,
                        ballot_t cur_term,
                        const function<void(Future *fu)> &callback);  
  shared_ptr<FpgaRaftPlusVote2FPGAQuorumEvent>
  BroadcastVote2FPGA(parid_t par_id,
                        slotid_t lst_log_idx,
                        ballot_t lst_log_term,
                        parid_t self_id,
                        ballot_t cur_term );
  void BroadcastVote2FPGA(parid_t par_id,
                        slotid_t lst_log_idx,
                        ballot_t lst_log_term,
                        parid_t self_id,
                        ballot_t cur_term,
                        const function<void(Future *fu)> &callback);  
  shared_ptr<FpgaRaftPlusAcceptQuorumEvent>
  BroadcastAccept(parid_t par_id,
                  slotid_t slot_id,
                  ballot_t ballot,
                  shared_ptr<Marshallable> cmd);
  void BroadcastAccept(parid_t par_id,
                       slotid_t slot_id,
                       ballot_t ballot,
                       shared_ptr<Marshallable> cmd,
                       const function<void(Future*)> &callback);
  shared_ptr<FpgaRaftPlusAppendQuorumEvent>
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
  void BroadcastAppendEntries(parid_t par_id,
                              slotid_t slot_id,
															i64 dep_id,
                              ballot_t ballot,
                              uint64_t currentTerm,
                              uint64_t prevLogIndex,
                              uint64_t prevLogTerm,
                              uint64_t commitIndex,
                              shared_ptr<Marshallable> cmd,
                              const function<void(Future*)> &callback);
  void BroadcastDecide(const parid_t par_id,
                       const slotid_t slot_id,
											 const i64 dep_id,
                       const ballot_t ballot,
                       const shared_ptr<Marshallable> cmd);
};

} // namespace janus

