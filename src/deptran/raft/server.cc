#include "server.h"
// #include "paxos_worker.h"
#include "exec.h"
#include "frame.h"
#include "coordinator.h"
#include "../classic/tpc_command.h"


namespace janus {

RaftServer::RaftServer(Frame * frame) {
  frame_ = frame ;
#ifdef RAFT_TEST_CORO
  setIsLeader(false);
#else
  setIsLeader(frame_->site_info_->locale_id == 0) ;
#endif
  stop_ = false ;
  timer_ = new Timer() ;
  
  // Start election timer immediately for test mode
#ifdef RAFT_TEST_CORO
  Log_debug("RaftServer constructor: failover_=%d, site_id_=%d", failover_, site_id_);
  if (failover_) {
    Log_debug("Starting election timer for server %d", site_id_);
    Coroutine::CreateRun([this](){
      Log_debug("Election timer coroutine started for server %d", site_id_);
      StartElectionTimer(); 
    });
  } else {
    Log_debug("Not starting election timer for server %d (failover_=false)", site_id_);
  }
#endif
}

void RaftServer::Setup() {
}

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
}

bool RaftServer::IsDisconnected() {
  return disconnected_;
}

void RaftServer::setIsLeader(bool isLeader) {
  Log_debug("set siteid %d is leader %d", site_id_, isLeader) ;
  is_leader_ = isLeader ;
  if (isLeader) {
    if (!failover_) {
      verify(frame_->site_info_->locale_id == 0);
      return;
    }
    // Reset leader volatile state
    RaftCommo *c = (RaftCommo*) commo();
    auto proxies = c->rpc_par_proxies_[partition_id_];
    for (auto& p : proxies) {
      if (p.first != site_id_) {
        // set matchIndex = 0
        match_index_[p.first] = 0;
        // set nextIndex = lastLogIndex + 1
        next_index_[p.first] = lastLogIndex + 1;
      }
    }
    // matchedIndex and nextIndex should have indices for all servers except self
    verify(match_index_.size() == Config::GetConfig()->GetPartitionSize(partition_id_) - 1);
    verify(next_index_.size() == Config::GetConfig()->GetPartitionSize(partition_id_) - 1);
  }
}

void RaftServer::applyLogs() {
  // This prevents the log entry from being applied twice
  if (in_applying_logs_) {
    return;
  }
  in_applying_logs_ = true;
  for (slotid_t id = executeIndex + 1; id <= commitIndex; id++) {
      auto next_instance = GetRaftInstance(id);
      if (next_instance->log_) {
          Log_debug("raft par:%d loc:%d executed slot %lx now", partition_id_, loc_id_, id);
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
}

void RaftServer::HeartbeatLoop() {
  auto hb_timer = new Timer();
  hb_timer->start();

  parid_t partition_id = partition_id_;
  if (!failover_) {
    auto proxies = commo()->rpc_par_proxies_[partition_id];
    for (auto& p : proxies) {
      if (p.first != loc_id_) {
        // set matchIndex = 0
        match_index_[p.first] = 0;
        // set nextIndex = 1
        next_index_[p.first] = 1;
      }
    }
    // matchedIndex and nextIndex should have indices for all servers except self
    verify(match_index_.size() == Config::GetConfig()->GetPartitionSize(partition_id) - 1);
    verify(next_index_.size() == Config::GetConfig()->GetPartitionSize(partition_id) - 1);
  }

  Log_debug("heartbeat loop init from site: %d", site_id_);
  looping_ = true;
  while(looping_) {
    uint64_t term;
    {
      {
        // std::lock_guard<std::recursive_mutex> lock(ready_for_replication_mtx_);
        // if (ready_for_replication_ == nullptr)
          ready_for_replication_ = Reactor::CreateSpEvent<IntEvent>();
#ifdef CURP_FULL_LOG_DEBUG
        Log_info("Before svr_->ready_for_replication_->Set(0);");
#endif
        ready_for_replication_->Set(0);
#ifdef CURP_FULL_LOG_DEBUG
        Log_info("After svr_->ready_for_replication_->Set(0);");
#endif
      }
#ifdef CURP_FULL_LOG_DEBUG
      Log_info("Before ready_for_replication_->Wait(HEARTBEAT_INTERVAL);");
#endif
      ready_for_replication_->Wait(HEARTBEAT_INTERVAL);
#ifdef CURP_FULL_LOG_DEBUG
      Log_info("After ready_for_replication_->Wait(HEARTBEAT_INTERVAL);");
#endif
      {
        // std::lock_guard<std::recursive_mutex> lock(ready_for_replication_mtx_);
        ready_for_replication_ = nullptr;
      }
      // Coroutine::Sleep(HEARTBEAT_INTERVAL);
      if (!IsLeader()) {
        continue;
      }
      // Log_info("time b/f sleep %" PRIu64, Time::now());
      // Coroutine::Sleep(HEARTBEAT_INTERVAL);
      // Log_info("time a/f sleep %" PRIu64, Time::now());
      auto nservers = Config::GetConfig()->GetPartitionSize(partition_id);
      for (auto it = next_index_.begin(); it != next_index_.end(); it++) {
        auto site_id = it->first;
        if (site_id == site_id_) {
          continue;
        }
        if (!IsLeader()) {
          // Log_info("sleep 1");
          // Log_info("wake 1");
          continue;
        }
        static uint64_t ttt = 0;
        uint64_t t2 = Time::now();
        if (ttt+1000000 < t2) {
          ttt = t2;
          Log_debug("heartbeat from site: %d", site_id);
          // Log_info("site %d in heartbeat_loop, not leader", site_id_);
        }
        mtx_.lock();
        // update commitIndex first
        std::vector<uint64_t> matchedIndices{};
        for (auto it = match_index_.begin(); it != match_index_.end(); it++) {
          matchedIndices.push_back(it->second);
        }
        verify(matchedIndices.size() == nservers - 1);
        std::sort(matchedIndices.begin(), matchedIndices.end());
        // new commitIndex is the (N/2 + 1)th largest index
        // only update commitIndex if the entry at new index was replicated in the current term
        uint64_t newCommitIndex = matchedIndices[(nservers - 1) / 2];
        verify(newCommitIndex <= lastLogIndex);
        if (newCommitIndex > commitIndex
            && (GetRaftInstance(newCommitIndex)->term == currentTerm)) {
          Log_debug("newCommitIndex %d", newCommitIndex);
          commitIndex = newCommitIndex;
        }
        // leader apply logs applicable
        if (commitIndex > executeIndex)
          applyLogs();

        term = currentTerm;
        mtx_.unlock();

      // send 1 AppendEntries to each follower that needs one
        // auto site_id = it->first;
        // if (site_id == site_id_) {
        //   continue;
        // }
        mtx_.lock();
        uint64_t prevLogIndex = it->second - 1;
        verify(prevLogIndex <= lastLogIndex);
        // if (prevLogIndex == lastLogIndex && !doHeartbeat) {
        //   continue;
        // }
        auto instance = GetRaftInstance(prevLogIndex);
        uint64_t prevLogTerm = instance->term;
        shared_ptr<Marshallable> cmd = nullptr;
        uint64_t cmdLogTerm = 0;

#ifndef RAFT_BATCH_OPTIMIZATION
        if (it->second <= lastLogIndex) {
          auto curInstance = GetRaftInstance(it->second);
          cmd = curInstance->log_;
          cmdLogTerm = curInstance->term;
          Log_debug("loc %d Sending AppendEntries for %d to loc %d cmd=%p",
              loc_id_, it->second, it->first, cmd.get());
        }
#endif

#ifdef RAFT_BATCH_OPTIMIZATION
        vector<shared_ptr<TpcCommitCommand> > batch_buffer_;
        for (int idx = it->second; idx <= lastLogIndex; idx++) {
          auto curInstance = GetRaftInstance(it->second);
          shared_ptr<TpcCommitCommand> curCmd = dynamic_pointer_cast<TpcCommitCommand>(curInstance->log_);
          curCmd->term = curInstance->term;
          batch_buffer_.push_back(curCmd);
        }
        // Log_info("batch size: %d", batch_buffer_.size());
        shared_ptr<TpcBatchCommand> batch_cmd = std::make_shared<TpcBatchCommand>();
        batch_cmd->AddCmds(batch_buffer_);

        if (batch_buffer_.size() > 0) {
          cmd = dynamic_pointer_cast<Marshallable>(batch_cmd);
        }
#endif

        uint64_t ret_status = false;
        uint64_t ret_term = 0;
        uint64_t ret_last_log_index = 0;
        mtx_.unlock();
        auto r = commo()->SendAppendEntries2(site_id,
                                            partition_id,
                                            -1,
                                            -1,
                                            IsLeader(),
                                            term,
                                            prevLogIndex,
                                            prevLogTerm,
                                            commitIndex,
                                            cmd,
                                            cmdLogTerm, // deprecated in batched version cmdLogTerm
                                            &ret_status,
                                            &ret_term,
                                            &ret_last_log_index);
        r->Wait(100000);
        if (r->status_ == Event::TIMEOUT) {
          continue;
        }
        mtx_.lock();
        auto& next_index = next_index_[site_id];
        auto& match_index = match_index_[site_id];
        if (ret_status == false & ret_term == 0 && ret_last_log_index == 0) {
          // do nothing
        } else if (currentTerm > term) {
          // continue; do nothing
        } else if (ret_status == 0 && ret_term > term) {
          // case 1: AppendEntries rejected because leader's term is expired
          if (currentTerm == term) {
            Log_debug("case 1: %d setting leader=false and currentTerm=%ld (received from %d)", loc_id_, ret_term, site_id);
            setIsLeader(false); // TODO problem here. When Raft requests votes, should it increase its term before sending requestvote?
            currentTerm = ret_term;
          }
        } else if (ret_status == 0) {
          // case 2: AppendEntries rejected because log doesn't contain an
          // entry at prevLogIndex whose term matches prevLogTerm
          // if (currentTerm > term)
          //   break;
          Log_debug("case 2: decrementing nextIndex (%ld)", next_index);
          next_index--; // todo: better backup
        } else {
          // case 3: AppendEntries accepted
          verify(ret_status == true);
          if (cmd == nullptr) {
            Log_debug("case 3A: AppendEntries accepted for heartbeat msg");
            verify(ret_term == term);
            // follower could have log entries after the prevLogIndex the AppendEntries was sent for.
            // neither party can detect if the entries are incorrect or not yet
            verify(ret_last_log_index >= next_index - 1);
            if (ret_last_log_index >= next_index) {
              if (next_index <= lastLogIndex) {
                next_index++;
                Log_debug("empty heartbeat incrementing next_index for site: %d, next_index: %d", site_id, next_index);
              }
            }
          } else {
            Log_debug("case 3B: AppendEntries accepted for non-empty msg");
            // follower could have log entries after the prevLogIndex the AppendEntries was sent for.
            // neither party can detect if the entries are incorrect or not yet
            verify(ret_last_log_index >= next_index);
            Log_debug("loc %ld followerLastLogIndex=%ld followerNextIndex=%ld followerMatchedIndex=%ld", 
                site_id, ret_last_log_index, next_index, match_index);
            match_index = next_index;
            // Log_info("About to update next_index %d to %d", next_index, ret_last_log_index + 1);
#ifndef RAFT_BATCH_OPTIMIZATION
            next_index++;
#endif
#ifdef RAFT_BATCH_OPTIMIZATION
            next_index = ret_last_log_index + 1;
#endif
            Log_debug("leader site %d receiving site %ld followerLastLogIndex=%ld followerNextIndex=%ld followerMatchedIndex=%ld", 
                site_id_, site_id, ret_last_log_index, next_index, match_index);
          }
        }
        mtx_.unlock();
      }
    }
	}
}

RaftServer::~RaftServer() {
  if (heartbeat_ && looping_) {
    looping_ = false;
	}
  
  stop_ = true ;
  Log_info("site par %d, loc %d: prepare %d, accept %d, commit %d", 
      partition_id_, loc_id_, n_prepare_, n_accept_, n_commit_);
}

bool RaftServer::RequestVote() {
  // for(int i = 0; i < 1000; i++) Log_info("not calling the wrong method");

  parid_t par_id = this->frame_->site_info_->partition_id_ ;
  parid_t loc_id = this->frame_->site_info_->locale_id ;

  uint32_t lstoff = 0  ;
  slotid_t lst_idx = 0 ;
  ballot_t lst_term = 0 ;

  {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    currentTerm++ ;
    lstoff = lastLogIndex - snapidx_ ;
    auto log = GetRaftInstance(lstoff) ; // causes min_active_slot_ verification error (server.h:247)
    lst_idx = lstoff + snapidx_ ;
    lst_term = log->term ;
  }
  
  auto term = currentTerm;
  Log_debug("raft server %d starting election for term %d, lastlogindex %d, lastlogterm %d", site_id_, term, lst_idx, lst_term);
  auto sp_quorum = ((RaftCommo *)(this->commo_))->BroadcastVote(par_id,lst_idx,lst_term,loc_id, term );
  sp_quorum->Wait(1000000);
  std::lock_guard<std::recursive_mutex> lock1(mtx_);
  if (sp_quorum->Yes()) {
    verify(currentTerm >= term);
    if (term != currentTerm) {
      return false;
    }
    // become a leader
    setIsLeader(true) ;
    verify(currentTerm == term);
    Log_debug("site %d became leader for term %d", site_id_, term);

    this->rep_frame_ = this->frame_ ;

    // auto co = ((TxLogServer *)(this))->CreateRepCoord(0);
    // auto empty_cmd = std::make_shared<TpcEmptyCommand>();
    // verify(empty_cmd->kind_ == MarshallDeputy::CMD_TPC_EMPTY);
    // auto sp_m = dynamic_pointer_cast<Marshallable>(empty_cmd);
    // ((CoordinatorRaft*)co)->Submit(sp_m);
    
    if(IsLeader()) {
	  	//for(int i = 0; i < 100; i++) Log_info("wait wait wait");
      Log_debug("vote accepted %d curterm %d", loc_id, currentTerm);
  		req_voting_ = false ;
			return true;
    } else {
      Log_debug("vote rejected %d curterm %d, do rollback", loc_id, currentTerm);
      setIsLeader(false) ;
    	return false;
		}
  } else if (sp_quorum->No()) {
    // become a follower
    Log_debug("site %d requestvote rejected", site_id_);
    setIsLeader(false) ;
    //reset cur term if new term is higher
    ballot_t new_term = sp_quorum->Term() ;
    currentTerm = new_term > currentTerm? new_term : currentTerm ;
  	req_voting_ = false ;
		return false;
  } else {
    Log_debug("vote timeout %d", loc_id);
  	req_voting_ = false ;
		return false;
  }
}

void RaftServer::OnRequestVote(const slotid_t& lst_log_idx,
                               const ballot_t& lst_log_term,
                               const siteid_t& can_id,
                               const ballot_t& can_term,
                               ballot_t *reply_term,
                               bool_t *vote_granted,
                               const function<void()> &cb) {
  std::lock_guard<std::recursive_mutex> lock(mtx_);
  Log_debug("raft receives vote from candidate: %llx", can_id);

  uint64_t cur_term = currentTerm ;
  if( can_term < cur_term)
  {
    doVote(lst_log_idx, lst_log_term, can_id, can_term, reply_term, vote_granted, false, cb) ;
    return ;
  }

  // has voted to a machine in the same term, vote no
  // TODO when to reset the vote_for_??
//  if( can_term == cur_term && vote_for_ != INVALID_PARID )
  if( can_term == cur_term)
  {
    doVote(lst_log_idx, lst_log_term, can_id, can_term, reply_term, vote_granted, false, cb) ;
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

  if( lst_log_term > curlstterm || (lst_log_term == curlstterm && lst_log_idx >= curlstidx) )
  {
    Log_debug("site %d vote for request vote from %d, lastidx %d, lastterm %d", site_id_, can_id, curlstidx, curlstterm);
    doVote(lst_log_idx, lst_log_term, can_id, can_term, reply_term, vote_granted, true, cb) ;
    return ;
  }

  doVote(lst_log_idx, lst_log_term, can_id, can_term, reply_term, vote_granted, false, cb) ;

}

void RaftServer::StartElectionTimer() {
  resetTimer() ;
  Coroutine::CreateRun([&]() {
    Log_debug("start timer for election") ;
    double duration = randDuration() ;
    while(!stop_) {
      Coroutine::Sleep(RandomGenerator::rand(5*HEARTBEAT_INTERVAL,10*HEARTBEAT_INTERVAL));
      auto time_now = Time::now();
      auto time_elapsed = time_now - last_heartbeat_time_;
      if (!IsLeader() && (time_now - last_heartbeat_time_ > 10 * HEARTBEAT_INTERVAL)) {
        Log_debug("site %d start election, time_elapsed: %d, last vote for: %d", 
          site_id_, time_elapsed, vote_for_);
        // ask to vote
        req_voting_ = true ;
        RequestVote() ;
        while(req_voting_) {
          Coroutine::Sleep(wait_int_);
          if(stop_) return ;
        }
      }
    } 
  });
}

bool RaftServer::Start(shared_ptr<Marshallable> &cmd,
                       uint64_t *index,
                       uint64_t *term,
                       slotid_t slot_id,
                       ballot_t ballot) {
  std::lock_guard<std::recursive_mutex> lock(mtx_);

  if (!heartbeat_setup_) {
    heartbeat_setup_ = true;
    if (heartbeat_) {
      Log_debug("starting heartbeat loop at site %d", site_id_);
      Coroutine::CreateRun([this](){
        this->HeartbeatLoop(); 
      });
      // Start election timeout loop
      if (failover_) {
        Coroutine::CreateRun([this](){
          StartElectionTimer(); 
        });
      }
    }
  }
  if (!IsLeader()) {
    *index = 0;
    *term = 0;
    return false;
  }
  SetLocalAppend(cmd, term, index, slot_id, ballot);
  // SetLocalAppend returns the old lastLogIndex value, but Start returns the
  // index of the newly appended instance
  verify(lastLogIndex == (*index) + 1);
  *index = lastLogIndex;
  Log_debug("Start(): ldr=%d index=%ld term=%ld", loc_id_, *index, *term);
  return true;
}

/* NOTE: same as ReceiveAppend */
/* NOTE: broadcast send to all of the host even to its own server 
 * should we exclude the execution of this function for leader? */
void RaftServer::OnAppendEntries(const slotid_t slot_id,
                                 const ballot_t ballot,
                                 const uint64_t leaderCurrentTerm,
                                 const uint64_t leaderPrevLogIndex,
                                 const uint64_t leaderPrevLogTerm,
                                 const uint64_t leaderCommitIndex,
                                 shared_ptr<Marshallable> &cmd,
                                 const uint64_t leaderNextLogTerm, // disabled in batched version (term recorded in the TpcCommitCommand)
                                 uint64_t *followerAppendOK,
                                 uint64_t *followerCurrentTerm,
                                 uint64_t *followerLastLogIndex,
                                 const function<void()> &cb) {
  std::lock_guard<std::recursive_mutex> lock(mtx_);
  Log_debug("on append entries for "
          "slot_id: %llx, site_id: %d, PrevLogIndex: %d lastLogIndex: %ld commitIndex: %ld",
          slot_id, (int)this->site_id_, leaderPrevLogIndex, lastLogIndex, commitIndex);
  if ((leaderCurrentTerm >= this->currentTerm) &&
        (leaderPrevLogIndex <= this->lastLogIndex) &&
        ((leaderPrevLogIndex == 0 ||
          GetRaftInstance(leaderPrevLogIndex)->term == leaderPrevLogTerm))) {
      Log_debug("refresh timer on appendentry");
      resetTimer() ;
      if (leaderCurrentTerm > this->currentTerm) {
          currentTerm = leaderCurrentTerm;
          Log_debug("server %d, set to be follower", loc_id_ ) ;
          setIsLeader(false) ;
      }

      if (cmd != nullptr) {
#ifndef RAFT_BATCH_OPTIMIZATION
        lastLogIndex = leaderPrevLogIndex + 1;
        auto instance = GetRaftInstance(lastLogIndex);
        instance->log_ = cmd;
        instance->term = leaderNextLogTerm;
#endif
#ifdef RAFT_BATCH_OPTIMIZATION
        auto cmds = dynamic_pointer_cast<TpcBatchCommand>(cmd);
        int cnt = 0;
        for (shared_ptr<TpcCommitCommand>& c: cmds->cmds_) {
          // SimpleRWCommand parsed_cmd = SimpleRWCommand(c);
          cnt++;
          lastLogIndex = leaderPrevLogIndex + cnt;
          auto instance = GetRaftInstance(lastLogIndex);
          instance->log_ = c;
          instance->term = dynamic_pointer_cast<TpcCommitCommand>(c)->term;
        }
#endif
      }

      // update commitIndex and apply logs if necessary
      if (leaderCommitIndex > commitIndex) {
        commitIndex = std::min(leaderCommitIndex, lastLogIndex);
        verify(lastLogIndex >= commitIndex);
        applyLogs();
      }

      *followerAppendOK = 1;
      *followerCurrentTerm = this->currentTerm;
      *followerLastLogIndex = this->lastLogIndex;

#ifndef RAFT_TEST_CORO
      if (cmd != nullptr) {
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

          // auto de = IO::write(filename, key_values, sizeof(struct KeyValue), kv_vector.size());
          // de->Wait();
        } else {
          int value = -1;
          // auto de = IO::write(filename, &value, sizeof(int), 1);
          // de->Wait();
        }
      }
#endif
    }
    else {
        Log_debug("reject append loc: %d, leader term %d last idx %d, last idx-term %d, server term: %d last idx: %d, last leader-idx-term %d",
            this->loc_id_, leaderCurrentTerm, leaderPrevLogIndex, leaderPrevLogTerm, currentTerm, lastLogIndex, GetRaftInstance(leaderPrevLogIndex)->term);        
        *followerAppendOK = 0;
        *followerCurrentTerm = this->currentTerm;
        *followerLastLogIndex = this->lastLogIndex;
    }

/*if (rand() % 1000 == 0) {
	usleep(25*1000);
}*/
    cb();
}

void RaftServer::removeCmd(slotid_t slot) {
  auto cmd = dynamic_pointer_cast<TpcCommitCommand>(raft_logs_[slot]->log_);
  if (!cmd)
    return;
  tx_sched_->DestroyTx(cmd->tx_id_);
  raft_logs_.erase(slot);
}

} // namespace janus
