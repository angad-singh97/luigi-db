#pragma once

#include "../__dep__.h"
#include "../constants.h"
#include "../scheduler.h"
#include "../classic/tpc_command.h"
#include "commo.h"

namespace janus {

#define HEARTBEAT_INTERVAL 100000

class Command;
class CmdData;

// #define INVALID_LOCID  ((parid_t)-1)
// #define NUM_BATCH_TIMER_RESET  (100)
// #define SEC_BATCH_TIMER_RESET  (1)

// struct RaftData {
//   ballot_t max_ballot_seen_ = 0;
//   ballot_t max_ballot_accepted_ = 0;
//   shared_ptr<Marshallable> accepted_cmd_{nullptr};
//   shared_ptr<Marshallable> committed_cmd_{nullptr};

//   ballot_t term;
//   shared_ptr<Marshallable> log_{nullptr};

// 	//for retries
// 	ballot_t prevTerm;
// 	slotid_t slot_id;
// };

// struct KeyValue {
// 	int key;
// 	i32 value;
// };

class RaftServer : public TxLogServer {
//  private:

//   bool is_leader_ = false;

//   // For timer and leader election
//   Timer *timer_;
//   bool stop_ = false;
//   bool init_ = false;
// #ifdef RAFT_LEADER_ELECTION_LOGIC
//   bool failover_{true};
// #endif
// #ifndef RAFT_LEADER_ELECTION_LOGIC
//   bool failover_{false};
// #endif

//   // For snapshot
//   slotid_t snapidx_ = 0;
//   ballot_t snapterm_ = 0;

// 	bool RequestVote() ;

// 	void Setup();
  
// 	void setIsLeader(bool isLeader)
//   {
//     Log_debug("set loc_id %d is leader %d", loc_id_, isLeader) ;
//     is_leader_ = isLeader ;
//     witness_.set_belongs_to_leader(isLeader);
//   }

  
//   void doVote(const slotid_t& lst_log_idx,
//                             const ballot_t& lst_log_term,
//                             const parid_t& can_id,
//                             const ballot_t& can_term,
//                             ballot_t *reply_term,
//                             bool_t *vote_granted,
//                             bool_t vote,
//                             const function<void()> &cb) {
// #ifdef RAFT_LEADER_ELECTION_DEBUG
//       Log_info("doVote %d vote %d vote_granted %d", loc_id_, can_id, vote);
// #endif
//       *vote_granted = vote ;
//       *reply_term = currentTerm ;
//       Log_debug("loc %d vote decision %d, for can_id %d canterm %d curterm %d isleader %d lst_log_idx %d lst_log_term %d", 
//             loc_id_, vote, can_id, can_term, currentTerm, is_leader_, lst_log_idx, lst_log_term );
                    
//       if( can_term > currentTerm)
//       {
//           // is_leader_ = false ;  // TODO recheck
//           currentTerm = can_term ;
//       }

//       if(vote)
//       {
//           setIsLeader(false) ;
//           vote_for_ = can_id ;
//           //reset timeout
//           //resetTimer() ;
//       }
//       cb() ;
//   }

//   void resetTimerBatch()
//   {
//     if (!failover_) return ;
//     auto cur_count = counter_++;
//     if (cur_count > NUM_BATCH_TIMER_RESET ) {
//       if (timer_->elapsed() > SEC_BATCH_TIMER_RESET) {
//         resetTimer();
//       }
//       counter_.store(0);
//     }
//   }

//   void resetTimer() {
//     if (failover_) timer_->start() ;
//   }

//   int32_t randDuration() 
//   {
//     return 4 + RandomGenerator::rand(0, 6) ;
//   }
//  public:

//   // Persistent state on all servers
//   uint64_t currentTerm = 0;
//   parid_t vote_for_ = INVALID_LOCID ;
//   map<slotid_t, shared_ptr<RaftData>> logs_{};
//   map<slotid_t, shared_ptr<RaftData>> raft_logs_{};

//   // Volatile state on all servers
//   slotid_t min_active_slot_ = 1; // anything before (lt) this slot is freed
//   slotid_t max_committed_slot_ = 0;
//   slotid_t max_executed_slot_ = 0;

//   // Volatile state on leaders
  
  

//   // Need to be understood
//   int32_t wait_int_ = 1 * 1000 * 1000 ; // 1s
//   bool in_applying_logs_ = false ;
//   atomic<int64_t> counter_{0};
//   /* NOTE: I think I should move these to the RaftData class */
//   /* TODO: talk to Shuai about it */
//   uint64_t lastLogIndex = 0;
//   uint64_t commitIndex = 0;
//   uint64_t executeIndex = 0;
  

//   void StartTimer() ;

//   bool IsLeader()
//   {
//     return is_leader_ ;
//   }

//   void SetLocalAppend(shared_ptr<Marshallable>& cmd, uint64_t* term, uint64_t* index, slotid_t slot_id = -1){
//     std::lock_guard<std::recursive_mutex> lock(mtx_);
//     *index = lastLogIndex ;
//     lastLogIndex += 1;
//     auto instance = GetRaftInstance(lastLogIndex);
//     instance->log_ = cmd;
// 		instance->prevTerm = currentTerm;
//     instance->term = currentTerm;
// 		instance->slot_id = slot_id;

//     if (cmd->kind_ == MarshallDeputy::CMD_TPC_COMMIT){
//       auto p_cmd = dynamic_pointer_cast<TpcCommitCommand>(cmd);
//       auto sp_vec_piece = dynamic_pointer_cast<VecPieceData>(p_cmd->cmd_)->sp_vec_piece_data_;
// 			vector<struct KeyValue> kv_vector;
// 			int index = 0;
// 			for (auto it = sp_vec_piece->begin(); it != sp_vec_piece->end(); it++){
// 				auto cmd_input = (*it)->input.values_;
// 				for (auto it2 = cmd_input->begin(); it2 != cmd_input->end(); it2++) {
// 					struct KeyValue key_value = {it2->first, it2->second.get_i32()};
// 					kv_vector.push_back(key_value);
// 				}
// 			}
//     } else {
//       // Do nothing
//     }
//     *term = currentTerm ;
//   }
  
//   shared_ptr<RaftData> GetInstance(slotid_t id) {
//     verify(id >= min_active_slot_);
//     auto& sp_instance = logs_[id];
//     if(!sp_instance)
//       sp_instance = std::make_shared<RaftData>();
//     return sp_instance;
//   }

//  /* shared_ptr<RaftData> GetRaftInstance(slotid_t id) {
//     if ( id <= raft_logs_.size() )
//     {
//         return raft_logs_[id-1] ;
//     }
//     auto sp_instance = std::make_shared<RaftData>();
//     raft_logs_.push_back(sp_instance) ;
//     return sp_instance;
//   }*/
//    shared_ptr<RaftData> GetRaftInstance(slotid_t id) {
//      verify(id >= min_active_slot_);
//      auto& sp_instance = raft_logs_[id];
//      if(!sp_instance)
//        sp_instance = std::make_shared<RaftData>();
//      return sp_instance;
//    }


  // RaftServer(Frame *frame) ;
  // ~RaftServer() ;

  // void OnRequestVote(const ballot_t& candidate_term,
  //                    const locid_t& candidate_id,
  //                    const uint64_t& last_log_index,
  //                    const ballot_t& last_log_term,
  //                    ballot_t* reply_term,
  //                    bool_t* vote_granted,
  //                    const function<void()> &cb);

  // void OnAppendEntries(const slotid_t slot_id,
  //                      const uint64_t leader_term,
  //                      const uint64_t leader_prev_log_index,
  //                      const uint64_t leader_prev_log_term,
  //                      shared_ptr<Marshallable> &cmd,
  //                      const uint64_t leader_commit_index,
  //                      uint64_t *follower_term,
  //                      uint64_t *follower_append_success,
  //                      uint64_t *follower_last_log_index,
  //                      const function<void()> &cb);

  // void OnCommit(const slotid_t slot_id,
  //               shared_ptr<Marshallable> &cmd);


public:

  class LogQueue {
    class Entry {
      public:
        shared_ptr<Marshallable> msg = nullptr;
        // Marshallable msg = Marshallable(0);
        uint64_t term;

        Entry(shared_ptr<Marshallable> &_msg, uint64_t _term) {
          msg = _msg;
          term = _term;
        }

        Entry() {
          // msg = nullptr;
          term = 0;
        }
  };
  std::vector<Entry> log;
  public:
    LogQueue() {
      log.clear();
      log.push_back(Entry());
    }

    uint64_t getLogSize() {
      return log.size() - 1;
    }

    Entry getLog(uint64_t idx) {
      return log[idx];
    }

    shared_ptr<Marshallable> getLogCmd(uint64_t idx) {
      // Log_info("[+] fetching %d from size %d", idx, log.size());
      auto msg = getLog(idx).msg;
      verify(msg != nullptr);
      return msg;
    }

    uint64_t getLogTerm(uint64_t idx) {
      return getLog(idx).term;
    }

    uint64_t getLastLogIndex() {
      return getLogSize();
    }

    uint64_t getLastLogTerm() {
      return getLogTerm(getLastLogIndex());
    }

    void deleteLogs(uint64_t from_idx) {
      log.erase(log.begin() + from_idx, log.end());
    }
  
    void addLog(shared_ptr<Marshallable> &cmd, uint64_t term) {
      verify(cmd != nullptr);
      log.push_back(Entry(cmd, term));
      verify(getLogCmd(getLastLogIndex()) != nullptr);

      // Log_info("[+] adding index %d | log size %d", getLastLogIndex(), log.size());
    }
  };

  const uint64_t IS_LEADER = 0;
  const uint64_t IS_CANDIDATE = 1;
  const uint64_t IS_FOLLOWER = 2;
  const uint64_t VOTED_FOR_NULL = -1;

  const uint64_t ELECTION_TIMEOUT_LOW = HEARTBEAT_INTERVAL * 9;
  const uint64_t ELECTION_TIMEOUT_HIGH = HEARTBEAT_INTERVAL * 12;

  const uint64_t BUSY_LOOP_SLEEP = HEARTBEAT_INTERVAL / 20;
  const uint64_t NETWORK_FAILED_TERM = 0;

  bool heatbeatReceived = false;
  uint64_t identity;

  uint64_t currentTerm;
  uint64_t votedFor;
  LogQueue logQueue;

  uint64_t commitIndex;
  uint64_t lastApplied;

  bool_t alive;

  vector<uint64_t> nextIndex;
  vector<uint64_t> matchIndex;
  
  // auto start = chrono::steady_clock::now();
  // while ((chrono::steady_clock::now() - start) < chrono::seconds{10}) {

  /* Your functions here */

  uint64_t getNumServers() {
    // return commo()->rpc_par_proxies_[partition_id_].size();
    return 5;
  }

  uint64_t getPartitionID() {
    return partition_id_;
  }

  uint64_t getThisServerID() {
    return this->loc_id_;
  }

  void initializeLeaderStates() {
    /*
      locid_t loc_id_;
      parid_t partition_id_;
      rpc_par_proxies_[par_id]
    */
    uint64_t numServers = getNumServers();
    nextIndex.resize(numServers);
    matchIndex.resize(numServers);

    for (auto i = 0; i < numServers; i++) {
      nextIndex[i] = logQueue.getLastLogIndex() + 1;
      matchIndex[i] = 0;
    }
  }

  void commitLogs() {
    while (lastApplied < commitIndex) {
      /* function<void(Marshallable &)> app_next_{}; */
      this->app_next_(*logQueue.getLogCmd(++lastApplied));
      // Log_info("[+] %d committed index %d term %d", getThisServerID(), lastApplied, logQueue.getLogTerm(lastApplied));
    }
  }

  // uint64_t getCurrentUs() {
  //   auto now = std::chrono::system_clock::now();
  //   auto now_t = std::chrono::system_clock::to_time_t(now);
  //   auto now_tm = std::localtime(&now_t);
  //   uint64_t us = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000000;
  //   return us;
  // }

  void updateCommitIndex() {
    if (identity != IS_LEADER) return ;
    
    for (auto i = logQueue.getLastLogIndex(); i > commitIndex; i--) {
      if (logQueue.getLogTerm(i) < currentTerm) 
        break;
      
      uint64_t c = 1;
      for (uint64_t id = 0; id < getNumServers(); id++) {
        if (id != getThisServerID() && matchIndex[id] >= i) 
          c += 1;
      }
      if (c > getNumServers() / 2) {
        // Log_info("[+] updateCommitIndex | start by %d | %d -> %d", getThisServerID(), commitIndex, i);

        commitIndex = i;
        break;
      }
    }
  }

  uint64_t generateRandomElectionTimeout() {
    // default_random_engine e;
    // uniform_int_distribution<uint64_t> u(ELECTION_TIMEOUT_LOW, ELECTION_TIMEOUT_HIGH);
    // return u(e);
    return rand() % (ELECTION_TIMEOUT_HIGH - ELECTION_TIMEOUT_LOW + 1) + ELECTION_TIMEOUT_LOW;
  }

  /* do not modify this class below here */

 public:
  RaftServer(Frame *frame) ;
  ~RaftServer() ;

  bool Start(shared_ptr<Marshallable> &cmd, uint64_t *index, uint64_t *term);
  void GetState(bool *is_leader, uint64_t *term);

 private:
  bool disconnected_ = false;
	void Setup();

 public:
  void SyncRpcExample();
  void Disconnect(const bool disconnect = true);
  void Reconnect() {
    Disconnect(false);
  }
  bool IsDisconnected();

  virtual bool HandleConflicts(Tx& dtxn,
                               innid_t inn_id,
                               vector<string>& conflicts) {
    verify(0);
  };

  // void removeCmd(slotid_t slot);

  RaftCommo* commo() {
    return (RaftCommo*)commo_;
  }
};
} // namespace janus
