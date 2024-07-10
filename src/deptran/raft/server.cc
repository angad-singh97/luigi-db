

#include "server.h"
// #include "paxos_worker.h"
#include "exec.h"
#include "frame.h"
#include "coordinator.h"
#include "../classic/tpc_command.h"


namespace janus {

RaftServer::RaftServer(Frame * frame) {
  frame_ = frame ;
  setIsLeader(frame_->site_info_->locale_id == 0) ;
  stop_ = false ;
  timer_ = new Timer() ;
}

void RaftServer::Setup() {
}

RaftServer::~RaftServer() {
		stop_ = true ;
}

bool RaftServer::RequestVote() {
  Log_info("not calling the wrong method");

  // currently don't request vote if no log
  if(this->commo_ == NULL || lastLogIndex == 0 ) return false;

  parid_t par_id = this->frame_->site_info_->partition_id_ ;
  parid_t loc_id = this->frame_->site_info_->locale_id ;

  Log_debug("fpga raft server %d in request vote", loc_id );

  uint32_t lstoff = 0  ;
  slotid_t last_idx = 0 ;
  ballot_t last_term = 0 ;

  {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    // TODO set fpga isleader false, recheck 
    currentTerm++ ;
    lstoff = lastLogIndex - snapidx_ ;
    auto log = GetRaftInstance(lstoff) ;
    last_idx = lstoff + snapidx_ ;
    last_term = log->term ;
  }
  
  auto sp_quorum = ((RaftCommo *)(this->commo_))->BroadcastRequestVote(par_id,currentTerm, loc_id, last_idx, last_term);
  sp_quorum->Wait();
  std::lock_guard<std::recursive_mutex> lock1(mtx_);
  if (sp_quorum->Yes()) {
    // become a leader
#ifdef RAFT_LEADER_ELECTION_DEBUG
    Log_info("loc_id=%d, setIsLeader %d", loc_id_, true);
#endif
    setIsLeader(true) ;

    this->rep_frame_ = this->frame_ ;

    auto co = ((TxLogServer *)(this))->CreateRepCoord(0);
    auto empty_cmd = std::make_shared<TpcEmptyCommand>();
    verify(empty_cmd->kind_ == MarshallDeputy::CMD_TPC_EMPTY);
    auto sp_m = dynamic_pointer_cast<Marshallable>(empty_cmd);
#ifdef RAFT_LEADER_ELECTION_DEBUG
    Log_info("before server %d submit empty cmd", loc_id_);
#endif
    ((CoordinatorRaft*)co)->Submit(sp_m);
#ifdef RAFT_LEADER_ELECTION_DEBUG
    Log_info("arrive here");
#endif
    if(IsLeader())
    {
	  	//for(int i = 0; i < 100; i++) Log_info("wait wait wait");
      Log_debug("vote accepted %d curterm %d", loc_id, currentTerm);
			return true;
    }
    else
    {
      Log_debug("fpga vote rejected %d curterm %d, do rollback", loc_id, currentTerm);
#ifdef RAFT_LEADER_ELECTION_DEBUG
      Log_info("loc_id=%d, setIsLeader %d", loc_id_, false);
#endif
      setIsLeader(false) ;
    	return false;
		}
  } else if (sp_quorum->No()) {
    // become a follower
    Log_debug("vote rejected %d", loc_id);
#ifdef RAFT_LEADER_ELECTION_DEBUG
    Log_info("loc_id=%d, setIsLeader %d", loc_id_, false);
#endif
    setIsLeader(false) ;
    //reset cur term if new term is higher
    ballot_t new_term = sp_quorum->Term() ;
    currentTerm = new_term > currentTerm? new_term : currentTerm ;
		return false;
  } else {
    // TODO process timeout.
    Log_debug("vote timeout %d", loc_id);
		return false;
  }
}

void RaftServer::OnRequestVote(const ballot_t& candidate_term,
                               const locid_t& candidate_id,
                               const uint64_t& last_log_index,
                               const ballot_t& last_log_term,
                               ballot_t* reply_term,
                               bool_t* vote_granted,
                               const function<void()> &cb) {

  std::lock_guard<std::recursive_mutex> lock(mtx_);
  Log_debug("fpga raft receives vote from candidate: %llx", candidate_id);

  // TODO wait all the log pushed to fpga host

  uint64_t cur_term = currentTerm ;
  if( candidate_term < cur_term)
  {
    doVote(last_log_index, last_log_term, candidate_id, candidate_term, reply_term, vote_granted, false, cb) ;
    return ;
  }

  // has voted to a machine in the same term, vote no
  // TODO when to reset the vote_for_??
//  if( candidate_term == cur_term && vote_for_ != INVALID_LOCID )
  if( candidate_term == cur_term)
  {
    doVote(last_log_index, last_log_term, candidate_id, candidate_term, reply_term, vote_granted, false, cb) ;
    return ;
  }

  // lstoff starts from 1
  uint32_t lstoff = lastLogIndex - snapidx_ ;

  ballot_t curlstterm = snapterm_ ;
  slotid_t curlstidx = lastLogIndex ;

  if(lstoff > 0 )
  {
    auto log = GetRaftInstance(lstoff) ;
    curlstterm = log->term ;
  }

  Log_debug("vote for lstoff %d, curlstterm %d, curlstidx %d", lstoff, curlstterm, curlstidx  );


  // TODO del only for test 
  verify(lstoff == lastLogIndex ) ;

  if( last_log_term > curlstterm || (last_log_term == curlstterm && last_log_index >= curlstidx) )
  {
    Log_info("OnRequestVote %d vote %d vote_granted since (last_log_term %d > curlstterm %d) || (last_log_term=curlstterm && last_log_index %d >= curlstidx %d)", loc_id_, candidate_id, last_log_term, curlstterm, last_log_index, curlstidx);
    doVote(last_log_index, last_log_term, candidate_id, candidate_term, reply_term, vote_granted, true, cb) ;
    return ;
  }

  doVote(last_log_index, last_log_term, candidate_id, candidate_term, reply_term, vote_granted, false, cb) ;

}

void RaftServer::StartTimer()
{
    if(!init_ ){
        resetTimer() ;
        Coroutine::CreateRun([&]() {
            Log_debug("start timer for election") ;
            int32_t duration = randDuration() ;
            while(!stop_)
            {
                if ( !IsLeader() && timer_->elapsed() > duration) {
                    Log_info("loc %d timer time out", loc_id_) ;
                    // ask to vote
                    RequestVote() ;
                    Log_debug("start a new timer") ;
                    resetTimer() ;
                    duration = randDuration() ;
                }
                auto sp_e2 = Reactor::CreateSpEvent<TimeoutEvent>(wait_int_);
                sp_e2->Wait() ;
            } 
        });
      init_ = true ;
  }
}

/* NOTE: same as ReceiveAppend */
/* NOTE: broadcast send to all of the host even to its own server 
 * should we exclude the execution of this function for leader? */
  void RaftServer::OnAppendEntries(const slotid_t slot_id,
                                     const uint64_t leader_term,
                                     const uint64_t leader_prev_log_index,
                                     const uint64_t leader_prev_log_term,
                                     shared_ptr<Marshallable> &cmd,
                                     const uint64_t leader_commit_index,
                                     uint64_t *follower_term,
                                     uint64_t *follower_append_success,
                                     uint64_t *follower_last_log_index,
                                     const function<void()> &cb) {
#ifdef RAFT_LEADER_ELECTION_DEBUG
        Log_info("OnAppendEntries svr %d", loc_id_);
#endif
        std::lock_guard<std::recursive_mutex> lock(mtx_);
#ifdef RAFT_LEADER_ELECTION_LOGIC
        StartTimer();
#endif
        
        Log_debug("fpga-raft scheduler on append entries for "
                "slot_id: %llx, loc: %d, PrevLogIndex: %d",
                slot_id, this->loc_id_, leader_prev_log_index);
        if ((leader_term >= this->currentTerm) &&
                (leader_prev_log_index <= this->lastLogIndex)
                /* TODO: log[leaderPrevLogidex].term == leader_prev_log_term */) {
            resetTimer() ;
            if (leader_term > this->currentTerm) {
                currentTerm = leader_term;
                Log_debug("server %d, set to be follower", loc_id_ ) ;
#ifdef RAFT_LEADER_ELECTION_DEBUG
                Log_info("loc_id=%d, setIsLeader %d", loc_id_, false);
#endif
                setIsLeader(false) ;
            }

						//this means that this is a retry of a previous one for a simulation
						/*if (slot_id == 100000000 || leader_prev_log_index + 1 < lastLogIndex) {
							for (int i = 0; i < 1000000; i++) Log_info("Dropping this AE message: %d %d", leader_prev_log_index, lastLogIndex);
							//verify(0);
							*follower_append_success = 0;
							cb();
							return;
						}*/
            verify(this->lastLogIndex == leader_prev_log_index);
            this->lastLogIndex = leader_prev_log_index + 1 /* TODO:len(ents) */;
            uint64_t prevCommitIndex = this->commitIndex;
            this->commitIndex = std::max(leader_commit_index, this->commitIndex);
            /* TODO: Replace entries after s.log[prev] w/ ents */
            /* TODO: it should have for loop for multiple entries */
            auto instance = GetRaftInstance(lastLogIndex);
            instance->log_ = cmd;


            // Pass the content to a thread that is always running
            // Disk write event
            // Wait on the event
            instance->term = this->currentTerm;
            //app_next_(*instance->log_); 
            verify(lastLogIndex > commitIndex);

            *follower_append_success = 1;
            *follower_term = this->currentTerm;
            *follower_last_log_index = this->lastLogIndex;
            
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

							struct KeyValue key_values[kv_vector.size()];
							std::copy(kv_vector.begin(), kv_vector.end(), key_values);
            } else {
							int value = -1;
            }
        }
        else {
            Log_debug("reject append loc: %d, leader term %d last idx %d, server term: %d last idx: %d",
                this->loc_id_, leader_term, leader_prev_log_index, currentTerm, lastLogIndex);          
            *follower_append_success = 0;
        }

				/*if (rand() % 1000 == 0) {
					usleep(25*1000);
				}*/
        WAN_WAIT
        cb();
    }

  void RaftServer::OnCommit(const slotid_t slot_id,
                              shared_ptr<Marshallable> &cmd) {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    // Log_info("OnCommit");
		struct timespec begin, end;
		//clock_gettime(CLOCK_MONOTONIC, &begin);

    // This prevents the log entry from being applied twice
    if (in_applying_logs_) {
      return;
    }
    in_applying_logs_ = true;
    
    for (slotid_t id = executeIndex + 1; id <= commitIndex; id++) {
        auto next_instance = GetRaftInstance(id);
        if (next_instance->log_) {
            Log_debug("fpga-raft par:%d loc:%d executed slot %lx now", partition_id_, loc_id_, id);
            // WAN_WAIT
            RuleWitnessGC(next_instance->log_);
            app_next_(*next_instance->log_);
            executeIndex++;
        } else {
            break;
        }
    }
    in_applying_logs_ = false;

    int i = min_active_slot_;
    while (i + 6000 < executeIndex) {
      removeCmd(i++);
    }
    min_active_slot_ = i;

		/*clock_gettime(CLOCK_MONOTONIC, &end);
		Log_info("time of decide on server: %d", (end.tv_sec - begin.tv_sec)*1000000000 + end.tv_nsec - begin.tv_nsec);*/
  }

  void RaftServer::removeCmd(slotid_t slot) {
    auto cmd = dynamic_pointer_cast<TpcCommitCommand>(raft_logs_[slot]->log_);
    if (!cmd)
      return;
    tx_sched_->DestroyTx(cmd->tx_id_);
    raft_logs_.erase(slot);
  }

} // namespace janus
