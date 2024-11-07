

#include "server.h"
// #include "paxos_worker.h"
#include "exec.h"
#include "frame.h"
#include "coordinator.h"
#include "../classic/tpc_command.h"


namespace janus {

// RaftServer::RaftServer(Frame * frame) {
//   frame_ = frame ;
//   setIsLeader(frame_->site_info_->locale_id == 0) ;
//   stop_ = false ;
//   timer_ = new Timer() ;
// }

// void RaftServer::Setup() {
//   SimpleRWCommand::SetZeroTime();
// }

// RaftServer::~RaftServer() {
// 		stop_ = true ;
// }

// bool RaftServer::RequestVote() {
//   Log_info("not calling the wrong method");

//   // currently don't request vote if no log
//   if(this->commo_ == NULL || lastLogIndex == 0 ) return false;

//   parid_t par_id = this->frame_->site_info_->partition_id_ ;
//   parid_t loc_id = this->frame_->site_info_->locale_id ;

//   Log_debug("fpga raft server %d in request vote", loc_id );

//   uint32_t lstoff = 0  ;
//   slotid_t last_idx = 0 ;
//   ballot_t last_term = 0 ;

//   {
//     std::lock_guard<std::recursive_mutex> lock(mtx_);
//     // TODO set fpga isleader false, recheck 
//     currentTerm++ ;
//     lstoff = lastLogIndex - snapidx_ ;
//     auto log = GetRaftInstance(lstoff) ;
//     last_idx = lstoff + snapidx_ ;
//     last_term = log->term ;
//   }
  
//   auto sp_quorum = ((RaftCommo *)(this->commo_))->BroadcastRequestVote(par_id,currentTerm, loc_id, last_idx, last_term);
//   sp_quorum->Wait();
//   std::lock_guard<std::recursive_mutex> lock1(mtx_);
//   if (sp_quorum->Yes()) {
//     // become a leader
// #ifdef RAFT_LEADER_ELECTION_DEBUG
//     Log_info("loc_id=%d, setIsLeader %d", loc_id_, true);
// #endif
//     setIsLeader(true) ;

//     this->rep_frame_ = this->frame_ ;

//     auto co = ((TxLogServer *)(this))->CreateRepCoord(0);
//     auto empty_cmd = std::make_shared<TpcEmptyCommand>();
//     verify(empty_cmd->kind_ == MarshallDeputy::CMD_TPC_EMPTY);
//     auto sp_m = dynamic_pointer_cast<Marshallable>(empty_cmd);
// #ifdef RAFT_LEADER_ELECTION_DEBUG
//     Log_info("before server %d submit empty cmd", loc_id_);
// #endif
//     ((CoordinatorRaft*)co)->Submit(sp_m);
// #ifdef RAFT_LEADER_ELECTION_DEBUG
//     Log_info("arrive here");
// #endif
//     if(IsLeader())
//     {
// 	  	//for(int i = 0; i < 100; i++) Log_info("wait wait wait");
//       Log_debug("vote accepted %d curterm %d", loc_id, currentTerm);
// 			return true;
//     }
//     else
//     {
//       Log_debug("fpga vote rejected %d curterm %d, do rollback", loc_id, currentTerm);
// #ifdef RAFT_LEADER_ELECTION_DEBUG
//       Log_info("loc_id=%d, setIsLeader %d", loc_id_, false);
// #endif
//       setIsLeader(false) ;
//     	return false;
// 		}
//   } else if (sp_quorum->No()) {
//     // become a follower
//     Log_debug("vote rejected %d", loc_id);
// #ifdef RAFT_LEADER_ELECTION_DEBUG
//     Log_info("loc_id=%d, setIsLeader %d", loc_id_, false);
// #endif
//     setIsLeader(false) ;
//     //reset cur term if new term is higher
//     ballot_t new_term = sp_quorum->Term() ;
//     currentTerm = new_term > currentTerm? new_term : currentTerm ;
// 		return false;
//   } else {
//     // TODO process timeout.
//     Log_debug("vote timeout %d", loc_id);
// 		return false;
//   }
// }

// void RaftServer::OnRequestVote(const ballot_t& candidate_term,
//                                const locid_t& candidate_id,
//                                const uint64_t& last_log_index,
//                                const ballot_t& last_log_term,
//                                ballot_t* reply_term,
//                                bool_t* vote_granted,
//                                const function<void()> &cb) {

//   std::lock_guard<std::recursive_mutex> lock(mtx_);
//   Log_debug("fpga raft receives vote from candidate: %llx", candidate_id);

//   // TODO wait all the log pushed to fpga host

//   uint64_t cur_term = currentTerm ;
//   if( candidate_term < cur_term)
//   {
//     doVote(last_log_index, last_log_term, candidate_id, candidate_term, reply_term, vote_granted, false, cb) ;
//     return ;
//   }

//   // has voted to a machine in the same term, vote no
//   // TODO when to reset the vote_for_??
// //  if( candidate_term == cur_term && vote_for_ != INVALID_LOCID )
//   if( candidate_term == cur_term)
//   {
//     doVote(last_log_index, last_log_term, candidate_id, candidate_term, reply_term, vote_granted, false, cb) ;
//     return ;
//   }

//   // lstoff starts from 1
//   uint32_t lstoff = lastLogIndex - snapidx_ ;

//   ballot_t curlstterm = snapterm_ ;
//   slotid_t curlstidx = lastLogIndex ;

//   if(lstoff > 0 )
//   {
//     auto log = GetRaftInstance(lstoff) ;
//     curlstterm = log->term ;
//   }

//   Log_debug("vote for lstoff %d, curlstterm %d, curlstidx %d", lstoff, curlstterm, curlstidx  );


//   // TODO del only for test 
//   verify(lstoff == lastLogIndex ) ;

//   if( last_log_term > curlstterm || (last_log_term == curlstterm && last_log_index >= curlstidx) )
//   {
// #ifdef RAFT_LEADER_ELECTION_DEBUG
//     Log_info("OnRequestVote %d vote %d vote_granted since (last_log_term %d > curlstterm %d) || (last_log_term=curlstterm && last_log_index %d >= curlstidx %d)", loc_id_, candidate_id, last_log_term, curlstterm, last_log_index, curlstidx);
// #endif
//     doVote(last_log_index, last_log_term, candidate_id, candidate_term, reply_term, vote_granted, true, cb) ;
//     return ;
//   }

//   doVote(last_log_index, last_log_term, candidate_id, candidate_term, reply_term, vote_granted, false, cb) ;

// }

// void RaftServer::StartTimer()
// {
//     if(!init_ ){
//         resetTimer() ;
//         Coroutine::CreateRun([&]() {
//             Log_debug("start timer for election") ;
//             int32_t duration = randDuration() ;
//             while(!stop_)
//             {
//                 if ( !IsLeader() && timer_->elapsed() > duration) {
//                     Log_info("loc %d timer time out", loc_id_) ;
//                     // ask to vote
//                     RequestVote() ;
//                     Log_debug("start a new timer") ;
//                     resetTimer() ;
//                     duration = randDuration() ;
//                 }
//                 auto sp_e2 = Reactor::CreateSpEvent<TimeoutEvent>(wait_int_);
//                 sp_e2->Wait() ;
//             } 
//         });
//       init_ = true ;
//   }
// }

// /* NOTE: same as ReceiveAppend */
// /* NOTE: broadcast send to all of the host even to its own server 
//  * should we exclude the execution of this function for leader? */
//   void RaftServer::OnAppendEntries(const slotid_t slot_id,
//                                      const uint64_t leader_term,
//                                      const uint64_t leader_prev_log_index,
//                                      const uint64_t leader_prev_log_term,
//                                      shared_ptr<Marshallable> &cmd,
//                                      const uint64_t leader_commit_index,
//                                      uint64_t *follower_term,
//                                      uint64_t *follower_append_success,
//                                      uint64_t *follower_last_log_index,
//                                      const function<void()> &cb) {
// #ifdef RAFT_LEADER_ELECTION_DEBUG
//         Log_info("OnAppendEntries svr %d", loc_id_);
// #endif
//         std::lock_guard<std::recursive_mutex> lock(mtx_);
// #ifdef RAFT_LEADER_ELECTION_LOGIC
//         StartTimer();
// #endif
        
//         Log_debug("fpga-raft scheduler on append entries for "
//                 "slot_id: %llx, loc: %d, PrevLogIndex: %d",
//                 slot_id, this->loc_id_, leader_prev_log_index);
//         if ((leader_term >= this->currentTerm) &&
//                 (leader_prev_log_index <= this->lastLogIndex)
//                 /* TODO: log[leaderPrevLogidex].term == leader_prev_log_term */) {
//             resetTimer() ;
//             if (leader_term > this->currentTerm) {
//                 currentTerm = leader_term;
//                 Log_debug("server %d, set to be follower", loc_id_ ) ;
// #ifdef RAFT_LEADER_ELECTION_DEBUG
//                 Log_info("loc_id=%d, setIsLeader %d", loc_id_, false);
// #endif
//                 setIsLeader(false) ;
//             }

// 						//this means that this is a retry of a previous one for a simulation
// 						/*if (slot_id == 100000000 || leader_prev_log_index + 1 < lastLogIndex) {
// 							for (int i = 0; i < 1000000; i++) Log_info("Dropping this AE message: %d %d", leader_prev_log_index, lastLogIndex);
// 							//verify(0);
// 							*follower_append_success = 0;
// 							cb();
// 							return;
// 						}*/
//             verify(this->lastLogIndex == leader_prev_log_index);
//             this->lastLogIndex = leader_prev_log_index + 1 /* TODO:len(ents) */;
//             uint64_t prevCommitIndex = this->commitIndex;
//             this->commitIndex = std::max(leader_commit_index, this->commitIndex);
//             /* TODO: Replace entries after s.log[prev] w/ ents */
//             /* TODO: it should have for loop for multiple entries */
//             auto instance = GetRaftInstance(lastLogIndex);
//             instance->log_ = cmd;


//             // Pass the content to a thread that is always running
//             // Disk write event
//             // Wait on the event
//             instance->term = this->currentTerm;
//             //app_next_(*instance->log_); 
//             verify(lastLogIndex > commitIndex);

//             *follower_append_success = 1;
//             *follower_term = this->currentTerm;
//             *follower_last_log_index = this->lastLogIndex;
            
// 						if (cmd->kind_ == MarshallDeputy::CMD_TPC_COMMIT){
//               auto p_cmd = dynamic_pointer_cast<TpcCommitCommand>(cmd);
//               auto sp_vec_piece = dynamic_pointer_cast<VecPieceData>(p_cmd->cmd_)->sp_vec_piece_data_;
              
// 							vector<struct KeyValue> kv_vector;
// 							int index = 0;
// 							for (auto it = sp_vec_piece->begin(); it != sp_vec_piece->end(); it++){
// 								auto cmd_input = (*it)->input.values_;
// 								for (auto it2 = cmd_input->begin(); it2 != cmd_input->end(); it2++) {
// 									struct KeyValue key_value = {it2->first, it2->second.get_i32()};
// 									kv_vector.push_back(key_value);
// 								}
// 							}

// 							struct KeyValue key_values[kv_vector.size()];
// 							std::copy(kv_vector.begin(), kv_vector.end(), key_values);
//             } else {
// 							int value = -1;
//             }
//         }
//         else {
//             Log_debug("reject append loc: %d, leader term %d last idx %d, server term: %d last idx: %d",
//                 this->loc_id_, leader_term, leader_prev_log_index, currentTerm, lastLogIndex);          
//             *follower_append_success = 0;
//         }

// 				/*if (rand() % 1000 == 0) {
// 					usleep(25*1000);
// 				}*/
//         WAN_WAIT
//         cb();
//     }

//   void RaftServer::OnCommit(const slotid_t slot_id,
//                               shared_ptr<Marshallable> &cmd) {
//     std::lock_guard<std::recursive_mutex> lock(mtx_);
//     // Log_info("OnCommit");
// 		struct timespec begin, end;
// 		//clock_gettime(CLOCK_MONOTONIC, &begin);

//     // This prevents the log entry from being applied twice
//     if (in_applying_logs_) {
//       return;
//     }
//     in_applying_logs_ = true;
    
//     for (slotid_t id = executeIndex + 1; id <= commitIndex; id++) {
//         auto next_instance = GetRaftInstance(id);
//         if (next_instance->log_) {
//             Log_debug("fpga-raft par:%d loc:%d executed slot %lx now", partition_id_, loc_id_, id);
//             // WAN_WAIT
//             RuleWitnessGC(next_instance->log_);
//             app_next_(*next_instance->log_);
//             executeIndex++;
//         } else {
//             break;
//         }
//     }
//     in_applying_logs_ = false;

//     int i = min_active_slot_;
//     while (i + 6000 < executeIndex) {
//       removeCmd(i++);
//     }
//     min_active_slot_ = i;

// 		/*clock_gettime(CLOCK_MONOTONIC, &end);
// 		Log_info("time of decide on server: %d", (end.tv_sec - begin.tv_sec)*1000000000 + end.tv_nsec - begin.tv_nsec);*/
//   }

  RaftServer::RaftServer(Frame * frame) {
  frame_ = frame ;
  /* Your code here for server initialization. Note that this function is 
     called in a different OS thread. Be careful about thread safety if 
     you want to initialize variables here. */

  srand(time(0));
  std::lock_guard<std::recursive_mutex> lk(mtx_);
  identity = IS_FOLLOWER;
  heatbeatReceived = false;
  currentTerm = 0;
  votedFor = VOTED_FOR_NULL;
  logQueue = LogQueue();
  commitIndex = 0;
  lastApplied = 0;
  alive = true;
}

RaftServer::~RaftServer() {
  /* Your code here for server teardown */
  std::lock_guard<std::recursive_mutex> lk(mtx_);
  alive = false;
}

void RaftServer::Setup() {
  /* Your code here for server setup. Due to the asynchronous nature of the 
     framework, this function could be called after a RPC handler is triggered. 
     Your code should be aware of that. This function is always called in the 
     same OS thread as the RPC handlers. */
  Log_info("[+] PAR %d | ID %d | Setup", this->getPartitionID(), this->getThisServerID());

  while (alive) {
    uint64_t identitySnapShot;
    {
      std::lock_guard<std::recursive_mutex> lk(mtx_);
      identitySnapShot = identity;
    }
    Log_info("After acquire lock");
    // if (identity == IS_CANDIDATE) {
    //   Log_info("[+] ElectionTimeOut, %d changing to candidate", getThisServerID());
    // }

    if (identitySnapShot == IS_FOLLOWER) {
      Log_info("identitySnapShot == IS_FOLLOWER");
      {
        std::lock_guard<std::recursive_mutex> lk(mtx_);
        heatbeatReceived = false;
      }

      auto electionTimeOutUs = generateRandomElectionTimeout();

      // Log_info("[+] follower %d into sleep for %d", getThisServerID(), electionTimeOutUs / 1000);

      Reactor::CreateSpEvent<TimeoutEvent>(electionTimeOutUs)->Wait();
      // Coroutine::Sleep(electionTimeOutUs);

      std::lock_guard<std::recursive_mutex> lk(mtx_);
      if (identity == IS_FOLLOWER && !heatbeatReceived) {
        identity = IS_CANDIDATE;
      }
    } else if (identitySnapShot == IS_CANDIDATE) {
      Log_info("identitySnapShot == IS_CANDIDATE");
        uint64_t termSnapShot, lastLogIndexSnapShot, lastLogTermSnapShot;
        {
          std::lock_guard<std::recursive_mutex> lk(mtx_);
          currentTerm ++;
          votedFor = this->getThisServerID();
          termSnapShot = currentTerm;
          lastLogIndexSnapShot = logQueue.getLastLogIndex();
          lastLogTermSnapShot = logQueue.getLastLogTerm();

          // Log_info("[+] PAR %d | ID %d | turn into candidate | term %d", 
          //   this->getPartitionID(), this->getThisServerID(), currentTerm);
        }
        auto electionTimeOutUs = generateRandomElectionTimeout();

        uint64_t quorum = 1, received = 1;

        /* sendVoteRequest */
        for (auto targetServer = 0; targetServer < getNumServers(); targetServer ++) {
          if (targetServer == this->getThisServerID()) continue;

          Coroutine::CreateRun([=, &quorum, &received](){
            uint64_t receiverTerm = -1;
            bool_t voteGranted = false;

            auto event = commo()->SendRequestVote(
              (parid_t) this->getPartitionID(),
              (siteid_t) targetServer,
              termSnapShot,
              (uint64_t) this->getThisServerID(),
              lastLogIndexSnapShot,
              lastLogTermSnapShot,
              &receiverTerm,
              &voteGranted
            );

            event->Wait(electionTimeOutUs);

            if (event->status_ != Event::TIMEOUT) {
              std::lock_guard<std::recursive_mutex> lk(mtx_);
              if (receiverTerm > currentTerm) {
                identity = IS_FOLLOWER;
                currentTerm = receiverTerm;
                votedFor = VOTED_FOR_NULL;
              } else {
                if (currentTerm == termSnapShot && identity == IS_CANDIDATE) {
                  if (voteGranted) {
                    // Log_info("[+] Received vote %d <- %d | Term %d", getThisServerID(), targetServer, currentTerm);
                    quorum ++;
                  }
                  received ++;
                } 
              }
            } else {
            }
            
          });
        }
        /* sendVoteRequest End */

        // Log_info("[+] Candidate %d | Term %d | start blocking for %d ms", getThisServerID(), currentTerm, electionTimeOutUs / 1000);
        uint64_t appBlockedUs = 0, busyLoopSleepUs = BUSY_LOOP_SLEEP;
        while (alive) {
          bool to_break = false;
          {
            std::lock_guard<std::recursive_mutex> lk(mtx_);
            // Log_info("[+] Candidate %d | Term %d | blocked %d/%d", 
            //   getThisServerID(), currentTerm, (getCurrentUs() - startVoteBlockedUs)/1000, electionTimeOutUs/1000);

            if (!(currentTerm == termSnapShot && identity == IS_CANDIDATE)) {
              // Log_info("[+] candidate %d exit with Term or Identity change", getThisServerID());
              to_break = true;
            } else if (quorum > getNumServers() / 2) {
              identity = IS_LEADER;
              // Log_info("[+] ID %d | Term %d | become leader | quorum %d/%d", 
              //   getThisServerID(), currentTerm, quorum, getNumServers());
              to_break = true;
            } else if (appBlockedUs >= electionTimeOutUs) {
              // Log_info("[+] candidate %d exit with electionTimeOutUs", getThisServerID());
              to_break = true;
            }
          }

          if (to_break) {
            break;
          }

          Coroutine::Sleep(busyLoopSleepUs);
          appBlockedUs += busyLoopSleepUs;
        }
        // Log_info("[+] Candidate %d | Term %d | end blocking", getThisServerID(), currentTerm);

    } else if (identitySnapShot == IS_LEADER) {
      Log_info("identitySnapShot == IS_LEADER");
      uint64_t termSnapShot;
      {
        std::lock_guard<std::recursive_mutex> lk(mtx_);
        initializeLeaderStates();
        termSnapShot = currentTerm;
      }

      Log_info("[+] in IS_LEADER branch | %d become leader | term %d", getThisServerID(), currentTerm);

      /* heartBeat */
      for (auto targetServer = 0; targetServer < getNumServers(); targetServer ++) {
        if (targetServer == this->getThisServerID()) continue;

        // Log_info("[+] heartbeat coro | %d -> %d", getThisServerID(), targetServer);

        Coroutine::CreateRun([=](){
          while (alive) {
            bool to_break = false;
            uint64_t leaderCommit, prevLogIndex, prevLogTerm;

            {
              std::lock_guard<std::recursive_mutex> lk(mtx_);
              if (currentTerm != termSnapShot || identity != IS_LEADER)
                to_break = true;
              else {
                leaderCommit = this->commitIndex;

                if (this->nextIndex[targetServer] > logQueue.getLastLogIndex())
                  prevLogIndex = this->nextIndex[targetServer] - 1;
                else 
                  prevLogIndex = this->nextIndex[targetServer];
                prevLogTerm = this->logQueue.getLogTerm(prevLogIndex);
              }
            }
            if (to_break) break;

            // Log_info("[+] here");

            uint64_t receiverTerm;
            bool_t success;

            // auto startUs = this->getCurrentUs();
            auto event = commo()->SendEmptyAppendEntries(
              (parid_t) this->getPartitionID(),
              (siteid_t) targetServer,
              termSnapShot,
              (uint64_t) this->getThisServerID(),
              prevLogIndex,
              prevLogTerm,
              leaderCommit,
              &receiverTerm,
              &success
            );

            // Log_info("[+] HeartBeat %d -> %d", getThisServerID(), targetServer);

            event->Wait(HEARTBEAT_INTERVAL);
            
            // Log_info("[+] HeartBeat %d -> %d end", getThisServerID(), targetServer);
            {
              std::lock_guard<std::recursive_mutex> lk(mtx_);

              if (event->status_ != Event::TIMEOUT) {
                if (receiverTerm > currentTerm) {
                  currentTerm = receiverTerm;
                  identity = IS_FOLLOWER;
                  votedFor = VOTED_FOR_NULL;
                } else if (currentTerm == termSnapShot && identity == IS_LEADER) {
                  if (success) {
                    this->matchIndex[targetServer] = max(prevLogIndex, this->matchIndex[targetServer]);
                    this->updateCommitIndex();
                    this->commitLogs();
                  }
                  else if (receiverTerm != NETWORK_FAILED_TERM) {
                    if (this->nextIndex[targetServer] != min(prevLogIndex, this->nextIndex[targetServer]))
                      // Log_info("[+] nextIndex %d | %d -> %d", 
                      //   targetServer, this->nextIndex[targetServer], min(prevLogIndex, this->nextIndex[targetServer]));

                    this->nextIndex[targetServer] = min(prevLogIndex, this->nextIndex[targetServer]);
                  }
                }
              }
            }

            Coroutine::Sleep(HEARTBEAT_INTERVAL);
          }
        });
      }
      /* heartBeat End*/
      
      /* replica */
      for (auto targetServer = 0; targetServer < getNumServers(); targetServer ++) {
        if (targetServer == this->getThisServerID()) continue;

        // Log_info("[+] creating replica coro: %d -> %d", getThisServerID(), targetServer);

        Coroutine::CreateRun([=](){
          while (alive) {
            bool to_break = false;

            uint64_t leaderTerm, leaderCommit, prevLogIndex, prevLogTerm;
            MarshallDeputy cmd; // MarshallDeputy md(cmd);
            uint64_t cmdTerm;
            // Marshallable cmd;
            bool to_send = false;

            {
              std::lock_guard<std::recursive_mutex> lk(mtx_);

              // Log_info("[+] checking Replica : %d -> %d", getThisServerID(), targetServer);

              if (currentTerm != termSnapShot || identity != IS_LEADER)
                to_break = true;
              else if (this->logQueue.getLastLogIndex() >= this->nextIndex[targetServer]) {
                to_send = true;
                leaderTerm = currentTerm;
                leaderCommit = commitIndex;
                prevLogIndex = this->nextIndex[targetServer] - 1;
                prevLogTerm = this->logQueue.getLogTerm(prevLogIndex);

                cmd = MarshallDeputy(this->logQueue.getLogCmd(this->nextIndex[targetServer]));
                cmdTerm = this->logQueue.getLogTerm(this->nextIndex[targetServer]);
              }
            }
            
            if (to_break) {
              // Log_info("[+] ending Replica Coro | %d -> %d", getThisServerID(), targetServer);
              break;
            }

            if (to_send) {
              uint64_t receiverTerm;
              bool_t success;

              // Log_info("[+] Replica Term %d | %d -> %d, index %d", currentTerm, getThisServerID(), targetServer, prevLogIndex + 1);
              
              auto event = commo()->SendAppendEntries(
                (parid_t) this->getPartitionID(),
                (siteid_t) targetServer,
                leaderTerm,
                (uint64_t) this->getThisServerID(),
                prevLogIndex,
                prevLogTerm,
                cmd,
                cmdTerm,
                leaderCommit,
                &receiverTerm,
                &success
              );

              event->Wait(BUSY_LOOP_SLEEP);

              // Log_info("[+] Replica Term %d | %d -> %d, index %d received status: %d", 
              //   currentTerm, getThisServerID(), targetServer, prevLogIndex + 1, event->status_);

              if (event->status_ != Event::TIMEOUT) {
                if (receiverTerm > currentTerm) {
                  identity = IS_FOLLOWER;
                  currentTerm = receiverTerm;
                  votedFor = VOTED_FOR_NULL;
                } else {
                  std::lock_guard<std::recursive_mutex> lk(mtx_);
                  if (currentTerm == leaderTerm && identity == IS_LEADER) {
                    if (success) {
                      this->matchIndex[targetServer] = max(prevLogIndex + 1, this->matchIndex[targetServer]);
                      this->nextIndex[targetServer] = max(prevLogIndex + 2, this->nextIndex[targetServer]);

                      // Log_info("[+] Term %d | %d -> %d succ | next idx to send: %d", currentTerm, getThisServerID(), targetServer, this->nextIndex[targetServer]);

                      this->updateCommitIndex();
                      this->commitLogs();
                    } else if (receiverTerm != NETWORK_FAILED_TERM) {
                      if (this->nextIndex[targetServer] != min(prevLogIndex, this->nextIndex[targetServer]))
                        // Log_info("[+] nextIndex %d | %d -> %d", 
                        //   targetServer, this->nextIndex[targetServer], min(prevLogIndex, this->nextIndex[targetServer]));

                      this->nextIndex[targetServer] = min(this->nextIndex[targetServer], prevLogIndex);
                    }
                  }
                }
              }

              if (!success)
                Coroutine::Sleep(BUSY_LOOP_SLEEP * 20);
            } else {
              Coroutine::Sleep(BUSY_LOOP_SLEEP);
            }
          }
        });
      }
      /* replica End */

      while (alive) {
        bool to_break = false;
        {
          std::lock_guard<std::recursive_mutex> lk(mtx_);
          if (!(identity == IS_LEADER && currentTerm == termSnapShot)) {
            // Log_info("[+] ID %d breaking! still leader: %d | currentTerm: %d | termSnapShot: %d",
            //   getThisServerID(), identity == IS_LEADER, currentTerm, termSnapShot);
            to_break = true;
          }
        }
        if (to_break) {
          // Log_info("[+] %d with Term %d break", getThisServerID(), currentTerm);
          // Log_info("[+] ID %d exist IS_LEADER branch ", getThisServerID());
          break;
        }
        Coroutine::Sleep(BUSY_LOOP_SLEEP);
      }

    }  
  }
}

bool RaftServer::Start(shared_ptr<Marshallable> &cmd,
                       uint64_t *index,
                       uint64_t *term) {
  /* Your code here. This function can be called from another OS thread. */
  std::lock_guard<std::recursive_mutex> lk(mtx_);
  if (identity != IS_LEADER)
    return false;

  // auto r_cmd = *cmd;
  // Marshallable x(0);
  // x = r_cmd;
  logQueue.addLog(cmd, currentTerm);
  *index = logQueue.getLastLogIndex();
  *term = currentTerm;

  // Log_info("[+] ID %d | Add cmd | index %d | term %d ", getThisServerID(), *index, *term);
  return true;
}

void RaftServer::GetState(bool *is_leader, uint64_t *term) {
  /* Your code here. This function can be called from another OS thread. */
  std::lock_guard<std::recursive_mutex> lk(mtx_);
  *is_leader = identity == IS_LEADER;
  *term = currentTerm;
}

void RaftServer::SyncRpcExample() {
  /* This is an example of synchronous RPC using coroutine; feel free to 
     modify this function to dispatch/receive your own messages. 
     You can refer to the other function examples in commo.h/cc on how 
     to send/recv a Marshallable object over RPC. */
  Coroutine::CreateRun([this](){
    string res;
    auto event = commo()->SendString(0, /* partition id is always 0 for lab1 */
                                     0, "hello", &res);
    event->Wait(1000000); //timeout after 1000000us=1s
    if (event->status_ == Event::TIMEOUT) {
      Log_info("timeout happens");
    } else {
      Log_info("rpc response is: %s", res.c_str()); 
    }
  });
}

/* Do not modify any code below here */

void RaftServer::Disconnect(const bool disconnect) {
  std::lock_guard<std::recursive_mutex> lock(mtx_);
  verify(disconnected_ != disconnect);
  // global map of rpc_par_proxies_ values accessed by partition then by site
  static map<parid_t, map<siteid_t, map<siteid_t, vector<SiteProxyPair>>>> _proxies{};
  if (_proxies.find(partition_id_) == _proxies.end()) {
    _proxies[partition_id_] = {};
  }
  RaftCommo *c = (RaftCommo*) commo();
  if (disconnect) {
    verify(_proxies[partition_id_][loc_id_].size() == 0);
    verify(c->rpc_par_proxies_.size() > 0);
    auto sz = c->rpc_par_proxies_.size();
    _proxies[partition_id_][loc_id_].insert(c->rpc_par_proxies_.begin(), c->rpc_par_proxies_.end());
    c->rpc_par_proxies_ = {};
    verify(_proxies[partition_id_][loc_id_].size() == sz);
    verify(c->rpc_par_proxies_.size() == 0);
  } else {
    verify(_proxies[partition_id_][loc_id_].size() > 0);
    auto sz = _proxies[partition_id_][loc_id_].size();
    c->rpc_par_proxies_ = {};
    c->rpc_par_proxies_.insert(_proxies[partition_id_][loc_id_].begin(), _proxies[partition_id_][loc_id_].end());
    _proxies[partition_id_][loc_id_] = {};
    verify(_proxies[partition_id_][loc_id_].size() == 0);
    verify(c->rpc_par_proxies_.size() == sz);
  }
  disconnected_ = disconnect;

  // if (disconnect)
  //   Log_info("[+] Server %d disconnected", getThisServerID());
  // else
  //   Log_info("[+] Server %d reconnected", getThisServerID());
}

bool RaftServer::IsDisconnected() {
  return disconnected_;
}

// void RaftServer::removeCmd(slotid_t slot) {
//   auto cmd = dynamic_pointer_cast<TpcCommitCommand>(raft_logs_[slot]->log_);
//   if (!cmd)
//     return;
//   tx_sched_->DestroyTx(cmd->tx_id_);
//   raft_logs_.erase(slot);
// }

} // namespace janus
