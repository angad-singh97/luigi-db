#pragma once

#include "../__dep__.h"
#include "../constants.h"
#include "../scheduler.h"
#include "../classic/tpc_command.h"

namespace janus {
class Command;
class CmdData;

#define INVALID_LOCID  ((parid_t)-1)
#define NUM_BATCH_TIMER_RESET  (100)
#define SEC_BATCH_TIMER_RESET  (1)

struct RaftData {
  ballot_t max_ballot_seen_ = 0;
  ballot_t max_ballot_accepted_ = 0;
  shared_ptr<Marshallable> accepted_cmd_{nullptr};
  shared_ptr<Marshallable> committed_cmd_{nullptr};

  ballot_t term;
  shared_ptr<Marshallable> log_{nullptr};

	//for retries
	ballot_t prevTerm;
	slotid_t slot_id;
};

struct KeyValue {
	int key;
	i32 value;
};

class RaftServer : public TxLogServer {
 private:

  bool is_leader_ = false;

  // For timer and leader election
  Timer *timer_;
  bool stop_ = false;
  bool init_ = false;
#ifdef RAFT_LEADER_ELECTION_LOGIC
  bool failover_{true};
#endif
#ifndef RAFT_LEADER_ELECTION_LOGIC
  bool failover_{false};
#endif

  // For snapshot
  slotid_t snapidx_ = 0;
  ballot_t snapterm_ = 0;

	bool RequestVote() ;

	void Setup();
  
	void setIsLeader(bool isLeader)
  {
    Log_debug("set loc_id %d is leader %d", loc_id_, isLeader) ;
    is_leader_ = isLeader ;
    witness_.set_belongs_to_leader(isLeader);
  }

  
  void doVote(const slotid_t& lst_log_idx,
                            const ballot_t& lst_log_term,
                            const parid_t& can_id,
                            const ballot_t& can_term,
                            ballot_t *reply_term,
                            bool_t *vote_granted,
                            bool_t vote,
                            const function<void()> &cb) {
      *vote_granted = vote ;
      *reply_term = currentTerm ;
      Log_debug("loc %d vote decision %d, for can_id %d canterm %d curterm %d isleader %d lst_log_idx %d lst_log_term %d", 
            loc_id_, vote, can_id, can_term, currentTerm, is_leader_, lst_log_idx, lst_log_term );
                    
      if( can_term > currentTerm)
      {
          // is_leader_ = false ;  // TODO recheck
          currentTerm = can_term ;
      }

      if(vote)
      {
          setIsLeader(false) ;
          vote_for_ = can_id ;
          //reset timeout
          //resetTimer() ;
      }
      cb() ;
  }

  void resetTimerBatch()
  {
    if (!failover_) return ;
    auto cur_count = counter_++;
    if (cur_count > NUM_BATCH_TIMER_RESET ) {
      if (timer_->elapsed() > SEC_BATCH_TIMER_RESET) {
        resetTimer();
      }
      counter_.store(0);
    }
  }

  void resetTimer() {
    if (failover_) timer_->start() ;
  }

  int32_t randDuration() 
  {
    return 4 + RandomGenerator::rand(0, 6) ;
  }
 public:

  // Persistent state on all servers
  uint64_t currentTerm = 0;
  parid_t vote_for_ = INVALID_LOCID ;
  map<slotid_t, shared_ptr<RaftData>> logs_{};
  map<slotid_t, shared_ptr<RaftData>> raft_logs_{};

  // Volatile state on all servers
  slotid_t min_active_slot_ = 1; // anything before (lt) this slot is freed
  slotid_t max_committed_slot_ = 0;
  slotid_t max_executed_slot_ = 0;

  // Volatile state on leaders
  
  

  // Need to be understood
  int32_t wait_int_ = 1 * 1000 * 1000 ; // 1s
  bool in_applying_logs_ = false ;
  atomic<int64_t> counter_{0};
  /* NOTE: I think I should move these to the RaftData class */
  /* TODO: talk to Shuai about it */
  uint64_t lastLogIndex = 0;
  uint64_t commitIndex = 0;
  uint64_t executeIndex = 0;
  

  void StartTimer() ;

  bool IsLeader()
  {
    return is_leader_ ;
  }

  void SetLocalAppend(shared_ptr<Marshallable>& cmd, uint64_t* term, uint64_t* index, slotid_t slot_id = -1){
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    *index = lastLogIndex ;
    lastLogIndex += 1;
    auto instance = GetRaftInstance(lastLogIndex);
    instance->log_ = cmd;
		instance->prevTerm = currentTerm;
    instance->term = currentTerm;
		instance->slot_id = slot_id;

    if (cmd->kind_ == MarshallDeputy::CMD_TPC_COMMIT){
      auto p_cmd = dynamic_pointer_cast<TpcCommitCommand>(cmd);
      auto sp_vec_piece = dynamic_pointer_cast<VecPieceData>(p_cmd->cmd_)->sp_vec_piece_data_;
			vector<struct KeyValue> kv_vector;
			int index = 0;
			for (auto it = sp_vec_piece->begin(); it != sp_vec_piece->end(); it++){
				auto cmd_input = (*it)->input.values_;
				for (auto it2 = cmd_input->begin(); it2 != cmd_input->end(); it2++) {
					struct KeyValue key_value = {it2->first, it2->second.get_i32()};
					kv_vector.push_back(key_value);
				}
			}
    } else {
      // Do nothing
    }
    *term = currentTerm ;
  }
  
  shared_ptr<RaftData> GetInstance(slotid_t id) {
    verify(id >= min_active_slot_);
    auto& sp_instance = logs_[id];
    if(!sp_instance)
      sp_instance = std::make_shared<RaftData>();
    return sp_instance;
  }

 /* shared_ptr<RaftData> GetRaftInstance(slotid_t id) {
    if ( id <= raft_logs_.size() )
    {
        return raft_logs_[id-1] ;
    }
    auto sp_instance = std::make_shared<RaftData>();
    raft_logs_.push_back(sp_instance) ;
    return sp_instance;
  }*/
   shared_ptr<RaftData> GetRaftInstance(slotid_t id) {
     verify(id >= min_active_slot_);
     auto& sp_instance = raft_logs_[id];
     if(!sp_instance)
       sp_instance = std::make_shared<RaftData>();
     return sp_instance;
   }


  RaftServer(Frame *frame) ;
  ~RaftServer() ;

  void OnRequestVote(const ballot_t& candidate_term,
                     const locid_t& candidate_id,
                     const uint64_t& last_log_index,
                     const ballot_t& last_log_term,
                     ballot_t* reply_term,
                     bool_t* vote_granted,
                     const function<void()> &cb);

  void OnAppendEntries(const slotid_t slot_id,
                       const uint64_t leader_term,
                       const uint64_t leader_prev_log_index,
                       const uint64_t leader_prev_log_term,
                       shared_ptr<Marshallable> &cmd,
                       const uint64_t leader_commit_index,
                       uint64_t *follower_term,
                       uint64_t *follower_append_success,
                       uint64_t *follower_last_log_index,
                       const function<void()> &cb);

  void OnCommit(const slotid_t slot_id,
                shared_ptr<Marshallable> &cmd);

  virtual bool HandleConflicts(Tx& dtxn,
                               innid_t inn_id,
                               vector<string>& conflicts) {
    verify(0);
  };

  void removeCmd(slotid_t slot);
};
} // namespace janus
