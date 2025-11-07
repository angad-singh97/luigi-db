#include "server.h"
// #include "paxos_worker.h"
#include "exec.h"
#include "frame.h"
#include "coordinator.h"
#include "../classic/tpc_command.h"
#include <cctype>

// @external: {
//   dynamic_pointer_cast: [unsafe, template<T, U>(const std::shared_ptr<U>& ptr) -> std::shared_ptr<T> where ptr: 'a, return: 'a]
//   make_shared: [unsafe, template<T, Args...>(Args&&... args) -> std::shared_ptr<T>]
//   min: [unsafe, template<T>(const T& a, const T& b) -> T]
//   copy: [unsafe, template<InputIt, OutputIt>(InputIt first, InputIt last, OutputIt d_first) -> OutputIt]
//   shared_ptr::operator bool: [unsafe, () -> bool]
//   shared_ptr::reset: [unsafe, (&'a mut) -> void]
//   operator bool: [unsafe, () -> bool]
//   sort: [unsafe, template<RandomIt>(RandomIt first, RandomIt last) -> void]
// }

namespace janus {

namespace {

bool JetpackRecoveryEnabled() {
  static const bool enabled = []() {
    const char* flag = std::getenv("MAKO_DISABLE_JETPACK");
    if (!flag || flag[0] == '\0') {
      return true;
    }
    std::string value(flag);
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
      return static_cast<char>(std::tolower(c));
    });

    auto is_true = [](const std::string& v) {
      return v == "1" || v == "true" || v == "yes" || v == "on";
    };
    auto is_false = [](const std::string& v) {
      return v == "0" || v == "false" || v == "no" || v == "off";
    };

    if (is_true(value)) {
      Log_info("[JETPACK-RUNTIME] MAKO_DISABLE_JETPACK=%s -> Jetpack recovery disabled", flag);
      return false;
    }
    if (is_false(value)) {
      return true;
    }

    Log_info("[JETPACK-RUNTIME] MAKO_DISABLE_JETPACK has unrecognised value '%s'; defaulting to disabled", flag);
    return false;
  }();
  return enabled;
}

}  // namespace

// @safe - Uses rusty::Box for timer ownership
RaftServer::RaftServer(Frame * frame)
  : timer_(rusty::Box<Timer>::make(Timer()))  // Initialize Box in member initializer list
{
  frame_ = frame ;
#ifdef RAFT_TEST_CORO
  setIsLeader(false);
#endif
  stop_ = false ;
}

// @safe - Calls undeclared Coroutine::CreateRun()
void RaftServer::Setup() {

#ifdef RAFT_TEST_CORO
  if (heartbeat_) {
		Log_debug("starting heartbeat loop at site %d", site_id_);
    Coroutine::CreateRun([this](){
      this->HeartbeatLoop();
    });
    // Start election timeout loop ONLY if not in fixed leader mode
    // In fixed leader mode, followers never start elections
    if (failover_ && !fixed_leader_mode_) {
      Coroutine::CreateRun([this](){
        StartElectionTimer();
      });
    } else if (fixed_leader_mode_) {
      Log_info("[RAFT-FIXED-LEADER] Site %d: Election timer DISABLED (fixed leader mode)", site_id_);
    }
	}
#endif

#ifndef RAFT_TEST_CORO
  if (heartbeat_) {
		Log_debug("starting heartbeat loop at site %d", site_id_);
    Coroutine::CreateRun([this](){
      this->HeartbeatLoop();
    });
    // Start election timeout loop ONLY if not in fixed leader mode
    // In fixed leader mode, followers never start elections
    if (failover_ && !fixed_leader_mode_) {
      Coroutine::CreateRun([this](){
        StartElectionTimer();
      });
    } else if (fixed_leader_mode_) {
      Log_info("[RAFT-FIXED-LEADER] Site %d: Election timer DISABLED (fixed leader mode)", site_id_);
    }
	}
#endif
  // Election timer will be started in Start() method when first command is submitted
}

void RaftServer::EnsureSetup() {
  if (heartbeat_setup_) {
    return;
  }
  heartbeat_setup_ = true;
  Setup();
}

// @safe
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

// @safe
bool RaftServer::IsDisconnected() {
  return disconnected_;
}

// void RaftServer::setIsLeader(bool isLeader) {
//   Log_info("set siteid %d is leader %d", frame_->site_info_->locale_id, isLeader) ;
  
//   // Log leader initialization when becoming a leader
//   if (isLeader) {
    
//     // CRITICAL FIX: Ensure lastLogIndex matches the highest index in raft_logs_
//     if (!raft_logs_.empty()) {
//       auto max_index = std::max_element(raft_logs_.begin(), raft_logs_.end(),
//                                        [](const auto& a, const auto& b) {
//                                          return a.first < b.first;
//                                        })->first;
//       if (max_index > lastLogIndex) {
//         lastLogIndex = max_index;
//       }
//     }
//   }
  
//   // Only update view when transitioning from non-leader to leader
//   if (isLeader && !is_leader_) {
//     // Only update view if we have enough information (not during initialization)
//     if (partition_id_ != 0xFFFFFFFF && site_id_ != -1 && frame_ != nullptr) {
//       // Move current new_view to old_view before updating
//       old_view_ = new_view_;
      
//       // Update new_view with this server as the leader
//       int n_replicas = Config::GetConfig()->GetPartitionSize(partition_id_);
//       new_view_ = View(n_replicas, site_id_, currentTerm);
//     }
//   } else if (!isLeader && is_leader_) {
//     // When transitioning from leader to non-leader
//     // View will be updated when we learn about the new leader
//   }
  
//   // Update the leader state after view handling
//   is_leader_ = isLeader;
  
//   if (isLeader) {
//     // JetpackRecovery();
//     // if (heartbeat_) {
//     //   Log_debug("starting heartbeat loop at site %d", site_id_);
//     //   Coroutine::CreateRun([this](){
//     //     this->HeartbeatLoop(); 
//     //   });
//     //   // Start election timeout loop
//     //   if (failover_) {
//     //     Coroutine::CreateRun([this](){
//     //       StartElectionTimer(); 
//     //     });
//     //   }
//     // }
//     // Log_info("!!!!!!! if (!failover_)");
//     // if (!failover_) {
//       // verify(frame_->site_info_->id == 0);
//       return;
//     // }
//     // Reset leader volatile state
//     RaftCommo *c = (RaftCommo*) commo();
//     auto proxies = c->rpc_par_proxies_[partition_id_];
    
//     // Clear existing indices first
//     match_index_.clear();
//     next_index_.clear();
    
//     for (auto& p : proxies) {
//       if (p.first != site_id_) {
//         // set matchIndex = 0
//         match_index_[p.first] = 0;
//         // set nextIndex = lastLogIndex + 1
//         next_index_[p.first] = lastLogIndex + 1;
//       }
//     }
//     // matchedIndex and nextIndex should have indices for all servers except self
//     verify(match_index_.size() == Config::GetConfig()->GetPartitionSize(partition_id_) - 1);
//     verify(next_index_.size() == Config::GetConfig()->GetPartitionSize(partition_id_) - 1);
//   }
// }

void RaftServer::RegisterLeaderChangeCallback(std::function<void(bool)> cb) {
  leader_change_cb_ = std::move(cb);
}

// @unsafe : Unsafe pointer address-of: pointer operations require unsafe context
void RaftServer::setIsLeader(bool isLeader) {
  bool prev_is_leader = is_leader_;
#ifdef RAFT_LEADER_ELECTION_DEBUG
  Log_info("[RAFT_STATE] setIsLeader invoked site %d (loc %d) term %lu: prev_is_leader=%d new_is_leader=%d",
           site_id_, frame_->site_info_->locale_id, currentTerm, prev_is_leader, isLeader);
#endif

  bool become_new_leader = isLeader && !is_leader_;
  bool become_new_follower = !isLeader && is_leader_;

  // Update leader flag immediately so subsequent logic sees the new state
  is_leader_ = isLeader;

  // Only update view when transitioning from non-leader to leader
  if (become_new_leader) {
    Log_info("[RAFT_STATE] setIsLeader transition LEADER: site %d term %lu prev_is_leader=%d become_new_leader=%d",
             site_id_, currentTerm, prev_is_leader, become_new_leader);
    // Only update view if we have enough information (not during initialization)
    if (partition_id_ != 0xFFFFFFFF && site_id_ != -1 && frame_ != nullptr) {
      // Move current new_view to old_view before updating
      old_view_ = new_view_;
      
      // Update new_view with this server as the leader
      int n_replicas = Config::GetConfig()->GetPartitionSize(partition_id_);
      new_view_ = View(n_replicas, site_id_, currentTerm);
      Log_info("[RAFT_VIEW] Server %d became leader for partition %d, term=%lu, old_view=%s, new_view=%s", 
               site_id_, partition_id_, currentTerm, 
               old_view_.ToString().c_str(), new_view_.ToString().c_str());
      
      // Ensure the communicator has the up-to-date leader view for this partition
      if (commo_ != nullptr) {
        auto view_data = std::make_shared<ViewData>(new_view_, partition_id_);
        commo()->UpdatePartitionView(partition_id_, view_data);
        Log_info("[RAFT_VIEW] Updated communicator view for partition %d with new leader %d",
                 partition_id_, site_id_);
      }
#ifndef RAFT_TEST_CORO
      if (JetpackRecoveryEnabled()) {
        JetpackRecoveryEntry();
      }
#endif
    }
  } else if (become_new_follower) {
    Log_info("[RAFT_STATE] setIsLeader transition FOLLOWER: site %d term %lu prev_is_leader=%d become_new_follower=%d",
             site_id_, currentTerm, prev_is_leader, become_new_follower);
    // When transitioning from leader to non-leader
    Log_info("[RAFT_VIEW] Server %d stepping down as leader for partition %d", site_id_, partition_id_);
    // View will be updated when we learn about the new leader
  }

  if (leader_change_cb_) {
    if (become_new_leader) {
      leader_change_cb_(true);
    } else if (become_new_follower) {
      leader_change_cb_(false);
    }
  }

  
  if (isLeader) {
    // Add null check for communicator
    if (commo_ == nullptr) {
      Log_info("commo_ is null, skipping leader initialization");
      return;
    }
    
    // JetpackRecovery();
    // if (heartbeat_) {
    //   Log_debug("starting heartbeat loop at site %d", site_id_);
    //   Coroutine::CreateRun([this](){
    //     this->HeartbeatLoop(); 
    //   });
    //   // Start election timeout loop
    //   if (failover_) {
    //     Coroutine::CreateRun([this](){
    //       StartElectionTimer(); 
    //     });
    //   }
    // }
    // Log_info("!!!!!!! if (!failover_)");
    // if (!failover_) {
    //   verify(frame_->site_info_->id == 0);
    //   return;
    // }
    // }
    // Reset leader volatile state
    RaftCommo *c = (RaftCommo*) commo();
    auto proxies = c->rpc_par_proxies_[partition_id_];
    if(failover_) {
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
}

// @unsafe - Unsafe pointer address-of in function call: pointer operations require unsafe context
void RaftServer::applyLogs() {
  // This prevents the log entry from being applied twice
  if (in_applying_logs_) {
    return;
  }
  in_applying_logs_ = true;

  for (slotid_t id = executeIndex + 1; id <= commitIndex; id++) {
    auto next_instance = GetRaftInstance(id);
    if (next_instance && next_instance->log_) {
      app_next_(id, next_instance->log_);
      executeIndex = id;
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

// @unsafe - Raw pointer allocation andManipulates raw reactor event pointers (ready_for_replication_) and reuses moved containers (matchedIndices/batch_buffer_/batch_cmd/cmd/r) during RPC batching; requires audit.
void RaftServer::HeartbeatLoop() {
  auto hb_timer = new Timer();
  hb_timer->start();

  parid_t partition_id = partition_id_;
  // Log_info("!!!!!!! if (!failover_)");
  // if (!failover_) {
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
  // }

  Log_debug("heartbeat loop init from site: %d", site_id_);
  looping_ = true;
  while(looping_) {
    uint64_t term;
    {
      {
        // std::lock_guard<std::recursive_mutex> lock(ready_for_replication_mtx_);
        // if (ready_for_replication_ == nullptr)
          ready_for_replication_ = Reactor::CreateSpEvent<IntEvent>();
        ready_for_replication_->Set(0);
      }
      ready_for_replication_->Wait(HEARTBEAT_INTERVAL);
      {
        // std::lock_guard<std::recursive_mutex> lock(ready_for_replication_mtx_);
        ready_for_replication_ = nullptr;
      }
      // Coroutine::Sleep(HEARTBEAT_INTERVAL);
      // Log_info("heartbeat loop at loc %d", loc_id_);
      if (!IsLeader()) {
        // Log_info("heartbeat loop at loc %d skip since not leader", loc_id_);
        continue;
      }
      // Log_info("[1]heartbeat loop at loc %d continue since is leader", loc_id_);
      // Log_info("time b/f sleep %" PRIu64, Time::now());
      // Coroutine::Sleep(HEARTBEAT_INTERVAL);
      // Log_info("time a/f sleep %" PRIu64, Time::now());
      auto nservers = Config::GetConfig()->GetPartitionSize(partition_id);
      // Log_info("next_index_ size %d", next_index_.size());
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
        // Log_info("[2]heartbeat loop at loc %d continue since is leader", loc_id_);
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
        
        // Debug logging for commitIndex calculation
        if (newCommitIndex > lastLogIndex) {
          if (!IsLeader()) {
            mtx_.unlock();
            continue;
          }
          Log_info("[COMMIT_INDEX_DEBUG] Leader %d: newCommitIndex=%ld > lastLogIndex=%ld", 
                   site_id_, newCommitIndex, lastLogIndex);
          Log_info("[COMMIT_INDEX_DEBUG] match_index_ values:");
          for (auto it = match_index_.begin(); it != match_index_.end(); it++) {
            Log_info("[COMMIT_INDEX_DEBUG]   server %d: match_index=%ld", it->first, it->second);
          }
          Log_info("[COMMIT_INDEX_DEBUG] matchedIndices sorted: ");
          for (size_t i = 0; i < matchedIndices.size(); i++) {
            Log_info("[COMMIT_INDEX_DEBUG]   [%zu]=%ld", i, matchedIndices[i]);
          }
          // Fix: cap newCommitIndex to lastLogIndex
          newCommitIndex = lastLogIndex;
          Log_info("[COMMIT_INDEX_DEBUG] Fixed newCommitIndex to %ld", newCommitIndex);
        }
        
        if (newCommitIndex > commitIndex && (GetRaftInstance(newCommitIndex)->term == currentTerm)) {
          Log_debug("newCommitIndex %d", newCommitIndex);
          commitIndex = newCommitIndex;
        }
        // leader apply logs applicable
        if (commitIndex > executeIndex)
          applyLogs();
        // Log_info("[3]heartbeat loop at loc %d continue since is leader", loc_id_);
        term = currentTerm;
        mtx_.unlock();

      // send 1 AppendEntries to each follower that needs one
        // auto site_id = it->first;
        // if (site_id == site_id_) {
        //   continue;
        // }
        mtx_.lock();
        uint64_t prevLogIndex = it->second - 1;
        if (prevLogIndex > lastLogIndex) {
          Log_info("[APPEND_ENTRIES] ERROR: prevLogIndex (%ld) > lastLogIndex (%ld), fixing next_index", prevLogIndex, lastLogIndex);
          // Fix the next_index to be valid
          it->second = lastLogIndex + 1;
          prevLogIndex = it->second - 1;
        }
        
        // Additional safety check: if prevLogIndex is still invalid, skip this follower
        if (prevLogIndex > lastLogIndex) {
          Log_info("[APPEND_ENTRIES] WARNING: Cannot send AppendEntries to follower %d: prevLogIndex (%ld) > lastLogIndex (%ld), skipping", 
                   site_id, prevLogIndex, lastLogIndex);
          // Reset the next_index to start from the beginning to allow the follower to catch up
          it->second = 1;
          continue;
        }
        
        verify(prevLogIndex <= lastLogIndex);
        // if (prevLogIndex == lastLogIndex && !doHeartbeat) {
        //   continue;
        // }
        auto instance = GetRaftInstance(prevLogIndex);

        // CRITICAL: Defensive null check
        if (!instance) {
          Log_error("[HEARTBEAT-SEND] [CRITICAL] GetRaftInstance(%lu) returned NULL! Skipping follower %d",
                    prevLogIndex, site_id);
          mtx_.unlock();
          continue;
        }

        uint64_t prevLogTerm = instance->term;
        shared_ptr<Marshallable> cmd = nullptr;
        uint64_t cmdLogTerm = 0;
        if (cmd != nullptr) {
          Log_info("[APPEND_ENTRIES] Leader %d: sending NEW log entry to follower %d, prevLogIndex=%ld, prevLogTerm=%ld, lastLogIndex=%ld", 
                   site_id_, site_id, prevLogIndex, prevLogTerm, lastLogIndex);
        }

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
        bool batch_failed = false;

        for (int idx = it->second; idx <= lastLogIndex; idx++) {
          auto curInstance = GetRaftInstance(idx);
          shared_ptr<TpcCommitCommand> curCmd = dynamic_pointer_cast<TpcCommitCommand>(curInstance->log_);

          // Check if the cast succeeded (is this a TpcCommitCommand?)
          if (!curCmd) {
            // Not a TpcCommitCommand (probably a simple test LogEntry)
            // Fall back to non-batch mode
            Log_info("[BATCH-FALLBACK] Non-TpcCommitCommand at idx=%d, falling back to single-entry mode", idx);
            batch_failed = true;
            break;
          }

          curCmd->term = curInstance->term;
          batch_buffer_.push_back(curCmd);
        }

        // Choose batch or fallback path based on what we found
        if (batch_failed) {
          // Fall back: send just the first entry (like non-batch mode)
          if (it->second <= lastLogIndex) {
            auto curInstance = GetRaftInstance(it->second);
            cmd = curInstance->log_;  // Works for any type: LogEntry, TpcCommitCommand, etc.
            cmdLogTerm = curInstance->term;
            Log_debug("[BATCH-FALLBACK] Sending single entry for idx=%d", it->second);
          }
        } else if (batch_buffer_.size() > 0) {
          // Normal batch path: all entries were TpcCommitCommand
          shared_ptr<TpcBatchCommand> batch_cmd = std::make_shared<TpcBatchCommand>();
          batch_cmd->AddCmds(batch_buffer_);
          cmd = dynamic_pointer_cast<Marshallable>(batch_cmd);
          Log_debug("[BATCH] Sending batch of %lu TpcCommitCommand entries", batch_buffer_.size());
        }
#endif

        uint64_t ret_status = false;
        uint64_t ret_term = 0;
        uint64_t ret_last_log_index = 0;
        mtx_.unlock();
        // Log_info("!!!!!!!!! SendAppendEntries2");
        auto r = commo()->SendAppendEntries2(site_id,
                                            partition_id,
                                            -1,
                                            -1,
                                            IsLeader(),
                                            site_id_,  // leader's site_id
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
            // Log_debug("case 1: %d setting leader=false and currentTerm=%ld (received from %d)", loc_id_, ret_term, site_id);
            setIsLeader(false); // TODO problem here. When Raft requests votes, should it increase its term before sending requestvote?
            currentTerm = ret_term;
          }
        } else if (ret_status == 0) {
          // case 2: AppendEntries rejected because log doesn't contain an
          // entry at prevLogIndex whose term matches prevLogTerm
          Log_debug("case 2: decrementing nextIndex (%ld)", next_index);
          if (next_index > 1) {
            next_index--; // todo: better backup
          } else {
            next_index = 1;
          }
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
#ifndef RAFT_BATCH_OPTIMIZATION
            match_index = next_index;
            next_index++;
#endif
#ifdef RAFT_BATCH_OPTIMIZATION
            // For batch optimization, match_index should be updated to the last index in the batch
            // which is ret_last_log_index, not next_index
            match_index = ret_last_log_index;
            next_index = ret_last_log_index + 1;
#endif
            // Safety check: ensure match_index doesn't exceed leader's lastLogIndex
            if (match_index > lastLogIndex) {
              Log_info("[MATCH_INDEX_DEBUG] Leader %d: capping match_index from %ld to %ld for follower %d", 
                       site_id_, match_index, lastLogIndex, site_id);
              match_index = lastLogIndex;
            }
            Log_debug("leader site %d receiving site %ld followerLastLogIndex=%ld followerNextIndex=%ld followerMatchedIndex=%ld", 
                site_id_, site_id, ret_last_log_index, next_index, match_index);
          }
        }
        mtx_.unlock();
      }
    }
	}
}


// @safe
RaftServer::~RaftServer() {
  // Stop the HeartbeatLoop
  if (heartbeat_ && looping_) {
    Log_info("[SHUTDOWN] Stopping HeartbeatLoop for site=%d", site_id_);
    looping_ = false;

    // Wake up the HeartbeatLoop if it's sleeping so it can see looping_=false
    if (ready_for_replication_) {
      ready_for_replication_->Set(1);
    }

    // Note: We cannot call Coroutine::Sleep() from destructor context.
    // The HeartbeatLoop will exit on its next iteration when it checks looping_.
    // This is a known limitation - there's a small race window, but it's better
    // than crashing in Sleep().
	}

  stop_ = true ;
  Log_info("site par %d, loc %d: prepare %d, accept %d, commit %d",
      partition_id_, loc_id_, n_prepare_, n_accept_, n_commit_);
}

// @safe
bool RaftServer::RequestVote() {
  // for(int i = 0; i < 1000; i++) Log_info("not calling the wrong method");

  parid_t par_id = this->frame_->site_info_->partition_id_ ;
  parid_t loc_id = this->frame_->site_info_->locale_id ;

  uint32_t lstoff = 0  ;
  slotid_t lst_idx = 0 ;
  ballot_t lst_term = 0 ;
  ballot_t prev_term = 0;
  siteid_t prev_vote_for = INVALID_SITEID;

  {
    std::lock_guard<std::recursive_mutex> lock(mtx_);
    prev_term = currentTerm;
    prev_vote_for = vote_for_;
    currentTerm++ ;
    lstoff = lastLogIndex - snapidx_ ;
    if (lstoff == 0) {
      lst_idx = snapidx_;
      lst_term = snapterm_;
    } else {
      auto log = GetRaftInstance(lstoff) ; // causes min_active_slot_ verification error (server.h:247)
      lst_idx = lstoff + snapidx_ ;
      lst_term = log->term ;
    }
  }
  
  auto term = currentTerm;
#ifdef RAFT_LEADER_ELECTION_DEBUG
  Log_info("[RAFT_ELECTION] server %d (loc %d) starting election term %lu->%lu lastLogIdx=%lu lastLogTerm=%lu prev_vote_for=%d",
           site_id_, loc_id, prev_term, term, lst_idx, lst_term, prev_vote_for);
#endif
  auto sp_quorum = ((RaftCommo *)(this->commo_))->BroadcastVote(par_id,lst_idx,lst_term,loc_id, term );
  sp_quorum->Wait(1000000);
  std::lock_guard<std::recursive_mutex> lock1(mtx_);
#ifdef RAFT_LEADER_ELECTION_DEBUG
  Log_info("[RAFT_ELECTION] server %d term %lu vote outcome yes=%d no=%d highest_term_seen=%ld timeout=%d",
           site_id_, term, sp_quorum->n_voted_yes_, sp_quorum->n_voted_no_, sp_quorum->Term(), sp_quorum->timeouted_);
#endif
  if (sp_quorum->Yes()) {
    verify(currentTerm >= term);
    if (term != currentTerm) {
#ifdef RAFT_LEADER_ELECTION_DEBUG
      Log_info("[RAFT_ELECTION] server %d abandoning leadership claim because local term advanced to %lu", site_id_, currentTerm);
#endif
      return false;
    }
    // become a leader
    setIsLeader(true) ;
    // verify(currentTerm == term); // [Jetpack] Disabled for recovery tests where term may advance asynchronously.
    Log_debug("site %d became leader for term %d", site_id_, term);

#ifdef RAFT_LEADER_ELECTION_DEBUG
    Log_info("[RAFT_ELECTION] server %d won election term %lu (votes yes=%d no=%d)",
             site_id_, term, sp_quorum->n_voted_yes_, sp_quorum->n_voted_no_);
#endif

    this->rep_frame_ = this->frame_ ;

    // auto co = ((TxLogServer *)(this))->CreateRepCoord(0);
    // auto empty_cmd = std::make_shared<TpcEmptyCommand>();
    // verify(empty_cmd->kind_ == MarshallDeputy::CMD_TPC_EMPTY);
    // auto sp_m = dynamic_pointer_cast<Marshallable>(empty_cmd);
    // ((CoordinatorRaft*)co)->Submit(sp_m);
    
    if(IsLeader()) {
	  	//for(int i = 0; i < 100; i++) Log_info("wait wait wait");
      Log_debug("vote accepted %d curterm %d", loc_id, currentTerm);
#ifdef RAFT_TEST_CORO
      // Skip JetpackRecovery in test environment to avoid RPC handler issues
#else
      if (JetpackRecoveryEnabled()) {
        Log_info("Triggering Jetpack recovery");
        JetpackRecoveryEntry(); // Trigger Jetpack recovery on new leader election
      }
#endif
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
#ifdef RAFT_LEADER_ELECTION_DEBUG
    Log_info("[RAFT_ELECTION] server %d lost election term %lu (yes=%d no=%d) highest_term=%ld",
             site_id_, term, sp_quorum->n_voted_yes_, sp_quorum->n_voted_no_, sp_quorum->Term());
#endif
    //reset cur term if new term is higher
    ballot_t new_term = sp_quorum->Term() ;
    currentTerm = new_term > currentTerm? new_term : currentTerm ;
  	req_voting_ = false ;
		return false;
  } else {
    Log_debug("vote timeout %d", loc_id);
#ifdef RAFT_LEADER_ELECTION_DEBUG
    Log_info("[RAFT_ELECTION] server %d election timed out term %lu (yes=%d no=%d)",
             site_id_, term, sp_quorum->n_voted_yes_, sp_quorum->n_voted_no_);
#endif
  	req_voting_ = false ;
		return false;
  }
}

// @safe - Calls undeclared doVote() and uses std::function callback
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

// @safe - Calls undeclared Coroutine::CreateRun()
void RaftServer::StartElectionTimer() {
  resetTimer() ;
  Coroutine::CreateRun([&]() {
    Log_debug("start timer for election") ;
    double duration = randDuration() ;
    while(!stop_) {
      Coroutine::Sleep(RandomGenerator::rand((frame_->site_info_->locale_id + 1) * 5*HEARTBEAT_INTERVAL,(frame_->site_info_->locale_id + 1) *10*HEARTBEAT_INTERVAL));
      auto time_now = Time::now();
      auto time_elapsed = time_now - last_heartbeat_time_;
      // Log_info("sleeped for %d ms bar %d ms", time_now - last_heartbeat_time_, 10 * HEARTBEAT_INTERVAL);
      if (!IsLeader() && (time_now - last_heartbeat_time_ > 10 * HEARTBEAT_INTERVAL)) {
        Log_debug("site %d start election, time_elapsed: %d, last vote for: %d", 
          site_id_, time_elapsed, vote_for_);
        // ask to vote
        req_voting_ = true ;
#ifdef RAFT_LEADER_ELECTION_DEBUG
        Log_info("[RAFT_TIMER] server %d triggering RequestVote() time_elapsed=%ld last_hb=%ld current_term=%lu vote_for=%d",
                 site_id_, time_elapsed, last_heartbeat_time_, currentTerm, vote_for_);
#endif
        RequestVote() ;
        while(req_voting_) {
          Coroutine::Sleep(wait_int_);
          if(stop_) return ;
        }
      }
    } 
  });
}

// @unsafe - Pointer operations "*index = lastLogIndex;"
bool RaftServer::Start(shared_ptr<Marshallable> &cmd,
                       uint64_t *index,
                       uint64_t *term,
                       slotid_t slot_id,
                       ballot_t ballot) {
  std::lock_guard<std::recursive_mutex> lock(mtx_);

  // #ifndef RAFT_TEST_CORO
  // if (!heartbeat_setup_) {
  //   heartbeat_setup_ = true;
  //   if (heartbeat_) {
  //     Log_debug("starting heartbeat loop at site %d", site_id_);
  //     Coroutine::CreateRun([this](){
  //       this->HeartbeatLoop(); 
  //     });
  //     // Start election timeout loop
  //     Log_info("!!!!!!! if (failover_)");
  //     if (failover_) {
  //       Coroutine::CreateRun([this](){
  //         StartElectionTimer(); 
  //       });
  //     }
  //   }
  // }
  // #endif
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
// @safe
void RaftServer::OnAppendEntries(const slotid_t slot_id,
                                 const ballot_t ballot,
                                 const uint64_t leaderCurrentTerm,
                                 const siteid_t leaderSiteId,
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
  // if (cmd != nullptr) {
  //   Log_debug("[APPEND_ENTRIES_RECEIVED] Follower %d: received NEW log entry from leader %d, leaderTerm=%ld, prevLogIndex=%ld, prevLogTerm=%ld, leaderCommit=%ld, currentTerm=%ld, lastLogIndex=%ld", 
  //            this->loc_id_, leaderSiteId, leaderCurrentTerm, leaderPrevLogIndex, leaderPrevLogTerm, leaderCommitIndex, currentTerm, lastLogIndex);
  // }
  bool term_ok = (leaderCurrentTerm >= this->currentTerm);
  bool index_ok = (leaderPrevLogIndex <= this->lastLogIndex);
  uint64_t local_prev_term = 0;
  if (leaderPrevLogIndex > 0 && leaderPrevLogIndex <= this->lastLogIndex) {
      local_prev_term = GetRaftInstance(leaderPrevLogIndex)->term;
  }
  bool prev_term_ok = (leaderPrevLogIndex == 0 || local_prev_term == leaderPrevLogTerm);

  if (term_ok && index_ok && prev_term_ok) {
      Log_debug("refresh timer on appendentry");
      resetTimer() ;
      if (leaderCurrentTerm > this->currentTerm) {
          currentTerm = leaderCurrentTerm;
          Log_debug("server %d, set to be follower", loc_id_ ) ;
          setIsLeader(false) ;
      }
      
      // // Update follower's view to track the current leader
      // if (!IsLeader() && leaderSiteId != INVALID_SITEID) {
      //     int prev_leader = new_view_.GetLeader();
      //     old_view_ = new_view_;
      //     int n_replicas = Config::GetConfig()->GetPartitionSize(partition_id_);
      //     new_view_ = View(n_replicas, leaderSiteId, leaderCurrentTerm);
      //     Log_info("[RAFT_VIEW_FOLLOWER] Server %d observed leader change %d->%d term=%lu prev_term=%lu",
      //              site_id_, prev_leader, leaderSiteId, leaderCurrentTerm, currentTerm);
      // }

      if (cmd != nullptr) {
#ifndef RAFT_BATCH_OPTIMIZATION
        lastLogIndex = leaderPrevLogIndex + 1;
        auto instance = GetRaftInstance(lastLogIndex);
        instance->log_ = cmd;
        instance->term = leaderNextLogTerm;
        // Log_debug("[APPEND_ENTRIES_ACCEPTED] Follower %d: accepted log entry at index %ld, term=%ld, lastLogIndex now=%ld", 
        //          this->loc_id_, lastLogIndex, leaderNextLogTerm, lastLogIndex);
        // // Log the command that was accepted
        // auto cmd_accepted = dynamic_pointer_cast<TpcCommitCommand>(cmd);
        // Log_debug("[APPEND_ENTRIES_ACCEPTED] Follower %d: accepted command %d at index %ld", 
        //          this->loc_id_, cmd_accepted ? cmd_accepted->tx_id_ : -1, lastLogIndex);
#endif
#ifdef RAFT_BATCH_OPTIMIZATION
        auto cmds = dynamic_pointer_cast<TpcBatchCommand>(cmd);
        int cnt = 0;
        for (shared_ptr<TpcCommitCommand>& c: cmds->cmds_) {
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

          // Check if this is Mako data (STR values) vs Janus data (I32 values)
          // Mako sends raw serialized transaction bytes wrapped as String values
          // This vestigial code was written for Janus I32 key-value pairs
          bool is_mako_data = false;
          if (sp_vec_piece && !sp_vec_piece->empty()) {
            auto first_cmd = (*sp_vec_piece)[0];
            if (first_cmd && first_cmd->input.values_ && !first_cmd->input.values_->empty()) {
              auto first_val = first_cmd->input.values_->begin()->second;
              if (first_val.get_kind() == Value::STR) {
                is_mako_data = true;
                Log_debug("[RAFT-ONAPPENDENTRIES] Skipping vestigial I/O code for Mako data (STR values)");
              }
            }
          }

          // Only process if this is Janus data (I32 values)
          // Skip for Mako data to avoid get_i32() crash on String values
          if (!is_mako_data) {
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
          }
        } else {
          int value = -1;
          // auto de = IO::write(filename, &value, sizeof(int), 1);
          // de->Wait();
        }
      }
#endif
    }
    else {
#ifdef RAFT_LEADER_ELECTION_DEBUG
        Log_info("[RAFT_APPEND_REJECT] follower=%d leader=%d leaderTerm=%lu localTerm=%lu prevIdx=%lu localLastIdx=%lu term_ok=%d index_ok=%d prev_term_ok=%d local_prev_term=%lu",
                 this->site_id_, leaderSiteId, leaderCurrentTerm, currentTerm, leaderPrevLogIndex, lastLogIndex,
                 term_ok, index_ok, prev_term_ok, local_prev_term);
#endif
        *followerAppendOK = 0;
        *followerCurrentTerm = this->currentTerm;
        *followerLastLogIndex = this->lastLogIndex;
    }

/*if (rand() % 1000 == 0) {
	usleep(25*1000);
}*/
    cb();
}

// @safe
void RaftServer::removeCmd(slotid_t slot) {
  auto cmd = dynamic_pointer_cast<TpcCommitCommand>(raft_logs_[slot]->log_);
  if (!cmd)
    return;
  tx_sched_->DestroyTx(cmd->tx_id_);
  raft_logs_.erase(slot);
}

} // namespace janus
