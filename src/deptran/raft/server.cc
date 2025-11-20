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
  // Record startup time for grace period logic
  startup_timestamp_ = Time::now();

#ifdef RAFT_TEST_CORO
  if (heartbeat_) {
		Log_debug("starting heartbeat loop at site %d", site_id_);
    Coroutine::CreateRun([this](){
      this->HeartbeatLoop();
    });
    // Start election timeout loop (with preferred replica bias)
    if (failover_) {
      Coroutine::CreateRun([this](){
        StartElectionTimer();
      });
    }
	}
#endif

#ifndef RAFT_TEST_CORO
  if (heartbeat_) {
		Log_debug("starting heartbeat loop at site %d", site_id_);
    Coroutine::CreateRun([this](){
      this->HeartbeatLoop();
    });
    // Start election timeout loop (with preferred replica bias)
    if (failover_) {
      Coroutine::CreateRun([this](){
        StartElectionTimer();
      });
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
    // ============================================================================
    // LEADERSHIP TRANSFER: Clear transfer flags when becoming leader
    // ============================================================================
    // If we just became leader, any previous transfer is now complete
    transferring_leadership_ = false;

    // If we're the preferred leader, also clear election suppression
    if (AmIPreferredLeader()) {
      suppress_election_for_transfer_.store(false, std::memory_order_release);
      Log_info("[LEADERSHIP-TRANSFER] Site %d (preferred): Became leader, cleared transfer flags", site_id_);
    }

    // ============================================================================
    // PREFERRED REPLICA: Stop catch-up monitoring when we become leader
    // ============================================================================
    // LEADERSHIP TRANSFER: Disabled monitoring thread, using piggybacked approach only
    // ============================================================================
    // The background monitoring thread (StartLeadershipTransferMonitoring) is disabled
    // because it doesn't work correctly with multi-partition systems where one RaftServer
    // manages multiple partitions with different preferred leaders.
    //
    // Instead, we rely entirely on the piggybacked transfer approach:
    // - Non-preferred leaders detect preferred replica catchup in HeartbeatLoop
    // - They piggyback trigger_election_now flag in AppendEntries
    // - Preferred replica receives signal and starts election
    // - Transfer completes in ~30ms
    //
    // This approach is partition-aware and works correctly in multi-partition systems.

    // if (!AmIPreferredLeader() && preferred_leader_site_id_ != INVALID_SITEID) {
    //   Log_info("[LEADERSHIP-TRANSFER] Site %d: I'm non-preferred leader, starting transfer monitoring",
    //            site_id_);
    //   StartLeadershipTransferMonitoring();
    // } else if (AmIPreferredLeader()) {
    //   Log_info("[LEADERSHIP-TRANSFER] Site %d: I'm preferred leader, no transfer needed", site_id_);
    // }

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
    // When transitioning from leader to non-leader
    Log_info("[RAFT_VIEW] Server %d stepping down as leader for partition %d", site_id_, partition_id_);

    // Stop leadership transfer monitoring if we were running it
    StopLeadershipTransferMonitoring();

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
  // Only mark pending if there's actually new work to apply
  if (executeIndex < commitIndex) {
    apply_pending_.store(true, std::memory_order_release);
  }

  // If already applying, return - the current apply loop will pick up our work
  if (in_applying_logs_) {
    return;
  }

  in_applying_logs_ = true;

  // Keep applying logs until no more pending work arrives
  // This ensures we never drop work even under heavy load
  do {
    // Clear the pending flag before processing
    apply_pending_.store(false, std::memory_order_release);

    // Apply all committed logs
    for (slotid_t id = executeIndex + 1; id <= commitIndex; id++) {
      auto next_instance = GetRaftInstance(id);
      if (next_instance && next_instance->log_) {
        app_next_(id, next_instance->log_);
        executeIndex = id;
      } else {
        break;
      }
    }

    // Check if new work arrived while we were applying
    // If so, loop again to process it
  } while (apply_pending_.load(std::memory_order_acquire));

  in_applying_logs_ = false;

  // Cleanup old commands to prevent memory buildup
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
      if (p.first == site_id_) {
        continue;  // skip self
      }
      // set matchIndex = 0
      match_index_[p.first] = 0;
      // set nextIndex = 1
      next_index_[p.first] = 1;
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
          // Fix: cap newCommitIndex to lastLogIndex
          newCommitIndex = lastLogIndex;
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
            // Handle case where follower is behind (e.g., during leadership transfer)
            if (ret_last_log_index + 1 < next_index) {
              Log_debug("Follower %d is behind (last_index=%lu, next_index=%lu), backing up next_index",
                        site_id, ret_last_log_index, next_index);
              next_index = ret_last_log_index + 1;
            } else if (ret_last_log_index >= next_index) {
              if (next_index <= lastLogIndex) {
                next_index++;
                Log_debug("empty heartbeat incrementing next_index for site: %d, next_index: %d", site_id, next_index);
              }
            }
          } else {
            Log_debug("case 3B: AppendEntries accepted for non-empty msg");
            // follower could have log entries after the prevLogIndex the AppendEntries was sent for.
            // neither party can detect if the entries are incorrect or not yet
            // Handle case where follower is behind (standard Raft backtracking)
            if (ret_last_log_index + 1 < next_index) {
              Log_debug("Follower %d lagging (last_index=%lu, next_index=%lu), backing up",
                        site_id, ret_last_log_index, next_index);
              next_index = ret_last_log_index + 1;
              match_index = ret_last_log_index;
              // Don't increment - let next heartbeat retry from backed-up position
              goto skip_increment;
            }
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
              match_index = lastLogIndex;
            }
skip_increment:  // Label for goto when we backtrack next_index
            Log_debug("leader site %d receiving site %ld followerLastLogIndex=%ld followerNextIndex=%ld followerMatchedIndex=%ld",
                site_id_, site_id, ret_last_log_index, next_index, match_index);
          }
        }
        mtx_.unlock();
      }
    }

    // ============================================================================
    // LEADERSHIP TRANSFER: Check if we should transfer to preferred replica
    // ============================================================================
    // After sending heartbeats to all followers, check if we're a non-preferred
    // leader and the preferred replica has caught up. If so, initiate transfer.
    //
    // This implements the piggybacked transfer approach in HeartbeatLoop instead
    // of using a separate monitoring thread (which doesn't work in multi-partition
    // systems due to global state conflicts).
    //
    // Check is done once per heartbeat cycle to avoid excessive overhead.
    if (IsLeader() && ShouldTransferLeadership()) {
      Log_info("[LEADERSHIP-TRANSFER] Site %d (partition %d): Initiating transfer from HeartbeatLoop",
               site_id_, partition_id_);
      InitiateLeadershipTransfer();
      // After initiating transfer, we'll step down and exit the heartbeat loop
      break;
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

  // Stop leadership transfer monitoring thread if running
  StopLeadershipTransferMonitoring();

  stop_ = true ;
  Log_info("site par %d, loc %d: prepare %d, accept %d, commit %d",
      partition_id_, loc_id_, n_prepare_, n_accept_, n_commit_);
}

// @safe
bool RaftServer::RequestVote() {
  // FIX 2: Prevent RequestVote during shutdown
  // The election timer coroutine may fire after ~RaftServer destructor runs,
  // causing a call to the base class TxLogServer::RequestVote() which hits verify(0)
  // Check stop_ flag to avoid this crash during teardown
  if (stop_) {
    Log_debug("[RAFT-SHUTDOWN] RequestVote called during shutdown (site=%d), ignoring to prevent crash", site_id_);
    return false;
  }

  // for(int i = 0; i < 1000; i++) Log_info("not calling the wrong method");
  uint64_t vote_start_time = Time::now();

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
  // CRITICAL FIX: Only reject if we already voted for someone else in this term
  // Standard Raft allows voting for the SAME candidate multiple times (idempotent)
  // and allows voting if we haven't voted yet in this term
  if( can_term == cur_term && vote_for_ != INVALID_SITEID && vote_for_ != can_id )
  {
    Log_debug("site %d vote NO for %d (already voted for %d in term %lu)",
              site_id_, can_id, vote_for_, cur_term);
    doVote(lst_log_idx, lst_log_term, can_id, can_term, reply_term, vote_granted, false, cb) ;
    return ;
  }

  // If we already voted for this same candidate in this term, vote YES again (idempotent)
  if( can_term == cur_term && vote_for_ == can_id )
  {
    Log_debug("site %d vote YES for %d (already voted for them in term %lu, idempotent)",
              site_id_, can_id, cur_term);
    doVote(lst_log_idx, lst_log_term, can_id, can_term, reply_term, vote_granted, true, cb) ;
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

  // ============================================================================
  // STANDARD RAFT VOTING (No Bias)
  // ============================================================================
  // Vote YES if candidate has at least as up-to-date logs as us
  // This is standard Raft - no preferred replica bias in voting
  // Leadership transfer will happen AFTER election via explicit protocol

  bool candidate_logs_up_to_date = (lst_log_term > curlstterm) ||
                                    (lst_log_term == curlstterm && lst_log_idx >= curlstidx);

  if (candidate_logs_up_to_date) {
    Log_debug("site %d vote YES for request vote from %d (logs up-to-date)", site_id_, can_id);
    doVote(lst_log_idx, lst_log_term, can_id, can_term, reply_term, vote_granted, true, cb);
  } else {
    Log_debug("site %d vote NO for request vote from %d (logs outdated)", site_id_, can_id);
    doVote(lst_log_idx, lst_log_term, can_id, can_term, reply_term, vote_granted, false, cb);
  }

}

// ============================================================================
// TIMEOUT-NOW RPC HANDLER - Leadership Transfer Protocol
// ============================================================================

/**
 * OnTimeoutNow - Handler for TimeoutNow RPC
 *
 * Receives instruction from current leader to start election immediately.
 * This is part of the leadership transfer protocol.
 *
 * Edge Cases Handled:
 * 1. Stale TimeoutNow from old term → Ignore
 * 2. Already leader → Ignore (already have leadership)
 * 3. Currently candidate → Ignore (already in election)
 * 4. Transferring leadership ourselves → Ignore (we're stepping down)
 * 5. Valid TimeoutNow → Start election immediately
 *
 * Safety: This maintains Raft safety because:
 * - We only start election if term is current (not stale)
 * - Standard election voting still applies
 * - We must win majority to become leader
 */
void RaftServer::OnTimeoutNow(const uint64_t leaderTerm,
                               const siteid_t leaderSiteId,
                               uint64_t *followerTerm,
                               bool_t *success,
                               const function<void()> &cb) {
  std::lock_guard<std::recursive_mutex> lock(mtx_);

  *followerTerm = currentTerm;
  *success = false;

  // ============================================================================
  // Edge Case 0: Server shutting down
  // ============================================================================
  if (stop_) {
    Log_info("[TIMEOUT-NOW] Site %d: Ignoring TimeoutNow - server shutting down", site_id_);
    cb();
    return;
  }

  // ============================================================================
  // Edge Case 1: Stale TimeoutNow from old term
  // ============================================================================
  if (leaderTerm < currentTerm) {
    Log_info("[TIMEOUT-NOW] Site %d: Ignoring stale TimeoutNow from leader %d (leader_term=%lu < my_term=%lu)",
             site_id_, leaderSiteId, leaderTerm, currentTerm);
    cb();
    return;
  }

  // ============================================================================
  // Edge Case 1b: Leader is ahead of us - update term
  // ============================================================================
  if (leaderTerm > currentTerm) {
    Log_info("[TIMEOUT-NOW] Site %d: Leader %d has higher term (%lu > %lu) - updating term and stepping down",
             site_id_, leaderSiteId, leaderTerm, currentTerm);

    currentTerm = leaderTerm;
    vote_for_ = INVALID_SITEID;  // Reset vote for new term

    if (is_leader_) {
      setIsLeader(false);  // Step down from leadership
    }

    *followerTerm = currentTerm;
  }

  // ============================================================================
  // Edge Case 2: Already leader
  // ============================================================================
  if (is_leader_) {
    Log_info("[TIMEOUT-NOW] Site %d: Ignoring TimeoutNow from leader %d - already leader in term %lu",
             site_id_, leaderSiteId, currentTerm);
    *success = true;  // Success = already leader (goal achieved)
    cb();
    return;
  }

  // ============================================================================
  // Edge Case 3: Currently candidate (already in election)
  // ============================================================================
  if (req_voting_) {
    Log_info("[TIMEOUT-NOW] Site %d: Ignoring TimeoutNow from leader %d - already requesting votes (term=%lu)",
             site_id_, leaderSiteId, currentTerm);
    *success = true;  // Success = already trying to become leader
    cb();
    return;
  }

  // ============================================================================
  // Edge Case 4: We're transferring leadership (stepping down)
  // ============================================================================
  if (transferring_leadership_) {
    Log_info("[TIMEOUT-NOW] Site %d: Ignoring TimeoutNow from leader %d - currently transferring leadership",
             site_id_, leaderSiteId);
    cb();
    return;
  }

  // ============================================================================
  // Valid TimeoutNow - Start Election Immediately
  // ============================================================================
  Log_info("[TIMEOUT-NOW] *** Site %d: Received TimeoutNow from leader %d (term=%lu) - STARTING ELECTION IMMEDIATELY ***",
           site_id_, leaderSiteId, leaderTerm);

  // Start election immediately (bypass random timeout)
  // This will increment term and send RequestVote RPCs
  bool election_started = RequestVote();

  if (election_started) {
    *success = true;
    Log_info("[TIMEOUT-NOW] Site %d: Election started successfully (new_term=%lu)",
             site_id_, currentTerm);
  } else {
    *success = false;
    Log_warn("[TIMEOUT-NOW] Site %d: Failed to start election",
             site_id_);
  }

  cb();
}



// @safe
uint64_t RaftServer::GetElectionTimeout() {
  uint64_t base_timeout;
  uint64_t current_time = Time::now();
  bool in_grace_period = (current_time - startup_timestamp_) < 5000000; // 5 seconds in microseconds

  if (AmIPreferredLeader()) {
    // Preferred replica: Short timeout (150-300ms) to win elections quickly
    base_timeout = 150000; // 150ms
    uint64_t jitter = RandomGenerator::rand(0, 150000);
    return base_timeout + jitter; // 150-300ms
  } else if (in_grace_period) {
    // Non-preferred during grace period: Long timeout (1-2s) to allow preferred to win
    base_timeout = 1000000; // 1s
    uint64_t jitter = RandomGenerator::rand(0, 1000000);
    return base_timeout + jitter; // 1-2s
  } else {
    // Non-preferred after grace: Medium timeout (500ms-1s) to enable failover
    base_timeout = 500000; // 500ms
    uint64_t jitter = RandomGenerator::rand(0, 500000);
    return base_timeout + jitter; // 500ms-1s
  }
}

// @safe - Calls undeclared Coroutine::CreateRun()
void RaftServer::StartElectionTimer() {
  resetTimer();
  last_heartbeat_time_ = Time::now();

  Coroutine::CreateRun([this]() {
    Log_debug("start timer for election") ;

    while(!stop_) {
      // Use dynamic election timeout based on preferred replica role and grace period
      uint64_t election_timeout = GetElectionTimeout();

      // Sleep for a portion of the timeout before checking
      Coroutine::Sleep(RandomGenerator::rand(HEARTBEAT_INTERVAL * 2, HEARTBEAT_INTERVAL * 4));

      auto time_now = Time::now();
      auto time_elapsed = time_now - last_heartbeat_time_;

      if (!IsLeader() && time_elapsed > election_timeout) {

        // ============================================================================
        // PHASE 2: CONDITIONAL ELECTION SUPPRESSION FOR NON-PREFERRED REPLICAS
        // ============================================================================
        // Non-preferred replicas check if preferred leader is still alive before
        // starting elections. This prevents election churn when heartbeats are
        // occasionally delayed under load (CPU/network/lock contention).
        //
        // Key insight: "Timeout fired" != "Leader is dead"
        // - Heartbeats can be delayed 500ms-1s under load
        // - Non-preferred timeout is 500ms-1s (competitive!)
        // - If preferred sent heartbeat within PREFERRED_ALIVE_THRESHOLD (1.5s),
        //   suppress election (preferred is alive, just slow)
        // - If no heartbeat for > 1.5s, allow election (preferred is dead, failover)
        //
        // This balances:
        // - Stable leadership (no churn from temporary delays)
        // - Fast failover (1.5s if preferred crashes)
        bool should_suppress = false;

        if (!AmIPreferredLeader() && preferred_leader_site_id_ != INVALID_SITEID) {
          // I'm non-preferred and we have a designated preferred leader
          // Check if preferred leader is still alive
          uint64_t time_since_preferred_heartbeat = time_now - last_heartbeat_from_preferred_time_;

          if (last_heartbeat_from_preferred_time_ > 0 &&
              time_since_preferred_heartbeat < PREFERRED_ALIVE_THRESHOLD) {
            // Preferred leader sent heartbeat recently (< 1.5s ago)
            // Even though my timeout fired, preferred is alive - suppress election
            should_suppress = true;

            // Log occasionally to track suppression
            static uint64_t last_phase2_log = 0;
            if (time_now - last_phase2_log > 5000000) {  // Log every 5s
              Log_info("[PHASE2-SUPPRESS] Site %d: Election suppressed - preferred leader %d alive "
                       "(last heartbeat %lu us ago, threshold %lu us)",
                       site_id_, preferred_leader_site_id_,
                       time_since_preferred_heartbeat, PREFERRED_ALIVE_THRESHOLD);
              last_phase2_log = time_now;
            }
          } else {
            // No recent heartbeat from preferred (> 1.5s or never received)
            // Preferred appears dead - allow election for failover
            if (last_heartbeat_from_preferred_time_ > 0) {
              Log_info("[PHASE2-FAILOVER] Site %d: Allowing election - no heartbeat from preferred %d "
                       "for %lu us (threshold %lu us)",
                       site_id_, preferred_leader_site_id_,
                       time_since_preferred_heartbeat, PREFERRED_ALIVE_THRESHOLD);
            }
          }
        }

        // ============================================================================
        // ELECTION SUPPRESSION DURING LEADERSHIP TRANSFER ONLY
        // ============================================================================
        // Standard Raft election logic: ALL replicas can start elections when they
        // timeout waiting for heartbeats. This ensures the system can always elect
        // a leader and maintain availability.
        //
        // ONLY suppress elections during active leadership transfer (when the
        // suppress_election_for_transfer_ flag is set). This flag is set when:
        // - A non-preferred follower receives a piggybacked transfer signal
        // - The transfer is in progress and we're waiting for preferred replica
        //
        // After a non-preferred replica wins an election, the background monitoring
        // thread (MonitorLeadershipAndTransfer) will detect this and initiate a
        // leadership transfer to the preferred replica.
        //
        // This approach:
        // - Maintains Raft safety and liveness (elections always possible)
        // - Achieves preferred leadership through transfer (not election suppression)
        // - Prevents cascading cross-partition deadlocks
        // - Works correctly with multiple independent Raft partitions

        // Only suppress during active leadership transfer
        if (suppress_election_for_transfer_.load(std::memory_order_acquire)) {
          should_suppress = true;

          // Check for transfer timeout (40 seconds)
          if (election_suppression_start_time_ > 0) {
            uint64_t suppression_duration = time_now - election_suppression_start_time_;
            const uint64_t TRANSFER_TIMEOUT = 40000000;  // 40 seconds

            if (suppression_duration > TRANSFER_TIMEOUT) {
              Log_info("[ELECTION-SUPPRESSION] Site %d: Transfer timeout expired (%lu us), clearing suppression",
                       site_id_, suppression_duration);
              suppress_election_for_transfer_.store(false, std::memory_order_release);
              election_suppression_start_time_ = 0;
              should_suppress = false;
            } else {
              // Log occasionally during suppression
              static uint64_t last_suppress_log = 0;
              if (time_now - last_suppress_log > 5000000) {
                Log_info("[ELECTION-SUPPRESSION] Site %d: Active transfer in progress, suppressing elections (%lu us elapsed)",
                         site_id_, suppression_duration);
                last_suppress_log = time_now;
              }
            }
          }
        }

        if (!should_suppress) {
          // ask to vote
          req_voting_ = true ;
#ifdef RAFT_LEADER_ELECTION_DEBUG
          Log_info("[RAFT_TIMER] server %d triggering RequestVote() time_elapsed=%ld last_hb=%ld current_term=%lu vote_for=%d",
                   site_id_, time_elapsed, last_heartbeat_time_, currentTerm, vote_for_);
#endif
          RequestVote() ;
        }
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

  // Reject new commands during leadership transfer to keep preferred replica caught up
  if (transferring_leadership_) {
    Log_debug("[LEADERSHIP-TRANSFER] Site %d: Rejecting new command during transfer", site_id_);
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
                                 const function<void()> &cb,
                                 bool trigger_election_now) {
  std::unique_lock<std::recursive_mutex> lock(mtx_);

  bool term_ok = (leaderCurrentTerm >= this->currentTerm);
  bool index_ok = (leaderPrevLogIndex <= this->lastLogIndex);
  uint64_t local_prev_term = 0;
  if (leaderPrevLogIndex > 0 && leaderPrevLogIndex <= this->lastLogIndex) {
      local_prev_term = GetRaftInstance(leaderPrevLogIndex)->term;
  }
  bool prev_term_ok = (leaderPrevLogIndex == 0 || local_prev_term == leaderPrevLogTerm);

  if (term_ok && index_ok && prev_term_ok) {
      Log_debug("refresh timer on appendentry");

      // DEBUG: Log timer reset
      // Log_info("[TIMER-RESET] Site %d (partition %d) | Heartbeat from Site %d reset election timer",
              //  site_id_, partition_id_, leaderSiteId);

      resetTimer() ;
      if (leaderCurrentTerm > this->currentTerm) {
          currentTerm = leaderCurrentTerm;
          Log_debug("server %d, set to be follower", loc_id_ ) ;
          setIsLeader(false) ;
      }

      // ============================================================================
      // ELECTION SUPPRESSION: Manage suppression during leadership transfer
      // ============================================================================
      // When we receive heartbeat from preferred leader: clear suppression AND transfer flag
      if (leaderSiteId == preferred_leader_site_id_ &&
          suppress_election_for_transfer_.load(std::memory_order_acquire)) {
        // Preferred leader is now active - clear suppression
        suppress_election_for_transfer_.store(false, std::memory_order_release);
        transferring_leadership_ = false;  // Transfer complete, can accept commands again
        Log_info("[ELECTION-SUPPRESSION] Site %d: Cleared (preferred leader %d now active)",
                 site_id_, preferred_leader_site_id_);
      }
      // When we receive heartbeat from NON-preferred leader: activate suppression
      // This tells us a transfer is likely in progress to the preferred replica
      else if (leaderSiteId != preferred_leader_site_id_ &&
               preferred_leader_site_id_ != INVALID_SITEID &&
               !AmIPreferredLeader() &&
               !suppress_election_for_transfer_.load(std::memory_order_acquire)) {
        // Non-preferred leader exists and we're also non-preferred
        // Activate suppression to prevent election storms during transfer
        suppress_election_for_transfer_.store(true, std::memory_order_release);
        election_suppression_start_time_ = Time::now();
        Log_info("[ELECTION-SUPPRESSION] Site %d: Activated (non-preferred leader %d detected, waiting for transfer to preferred %d)",
                 site_id_, leaderSiteId, preferred_leader_site_id_);
      }

      // ============================================================================
      // PREFERRED REPLICA: Track leader's commit index for catch-up detection
      // ============================================================================
      // Update our view of the leader's commit progress.
      // The preferred replica uses this to detect when it has caught up.
      if (leaderCommitIndex > leader_last_commit_index_) {
        leader_last_commit_index_ = leaderCommitIndex;
        Log_debug("[PREFERRED-REPLICA] Site %d: Updated leader commit index to %lu (my commit: %lu)",
                  site_id_, leader_last_commit_index_, commitIndex);
      }

      // ============================================================================
      // PHASE 2: Track heartbeat from preferred leader for election suppression
      // ============================================================================
      // Update timestamp when we receive heartbeat from preferred leader.
      // Non-preferred replicas use this to avoid starting elections when
      // preferred leader is alive but heartbeats are delayed under load.
      if (IsPreferredLeader(leaderSiteId)) {
        last_heartbeat_from_preferred_time_ = Time::now();
        Log_debug("[PHASE2] Site %d: Received heartbeat from preferred leader %d",
                  site_id_, leaderSiteId);
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

      // update commitIndex and trigger log application
      bool need_apply = false;
      if (leaderCommitIndex > commitIndex) {
        commitIndex = std::min(leaderCommitIndex, lastLogIndex);
        verify(lastLogIndex >= commitIndex);
        need_apply = true;
      }

      *followerAppendOK = 1;
      *followerCurrentTerm = this->currentTerm;
      *followerLastLogIndex = this->lastLogIndex;

      // CRITICAL FIX: Release mutex before applying logs!
      // This allows concurrent AppendEntries to be processed
      // while we're applying the current batch
      lock.unlock();

      if (need_apply) {
        applyLogs();  // Now called WITHOUT holding the mutex!
      }

      // Re-acquire mutex before returning (to handle remaining code safely)
      lock.lock();

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
        // DEBUG: Log why heartbeat was rejected
        Log_info("[HB-REJECT] Site %d ← Site %d | REJECTED | term_ok=%d index_ok=%d prev_term_ok=%d | leaderTerm=%lu myTerm=%lu",
                 site_id_, leaderSiteId, term_ok, index_ok, prev_term_ok, leaderCurrentTerm, currentTerm);

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

    // ============================================================================
    // PIGGYBACKED LEADERSHIP TRANSFER: Handle trigger_election_now flag
    // ============================================================================
    // The leader sends trigger_election_now=true to ALL replicas during transfer.
    // How we handle it depends on whether we're the preferred replica or not.
    if (trigger_election_now) {
        if (AmIPreferredLeader()) {
            // I'm the PREFERRED replica - start election (if not already leader)
            if (!IsLeader()) {
                Log_info("[PIGGYBACKED-TRANSFER] Site %d (preferred): Received transfer signal from leader %d - will start election after 30ms",
                         site_id_, leaderSiteId);

                // Wait before starting election to allow old leader's heartbeats
                // to reach other replicas. This prevents election storms.
                Coroutine::CreateRun([this]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(30));
                    RequestVote();
                });
            } else {
                Log_info("[PIGGYBACKED-TRANSFER] Site %d (preferred): Received transfer signal but already leader - ignoring",
                         site_id_);
            }
        } else {
            // I'm a NON-PREFERRED replica - activate election suppression
            if (!suppress_election_for_transfer_.load(std::memory_order_acquire)) {
                suppress_election_for_transfer_.store(true, std::memory_order_release);
                election_suppression_start_time_ = Time::now();
                Log_info("[PIGGYBACKED-TRANSFER] Site %d (non-preferred): Received transfer signal - activating election suppression (preferred=%d)",
                         site_id_, preferred_leader_site_id_);
            }
        }
    }

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

// ============================================================================
// PREFERRED REPLICA SYSTEM IMPLEMENTATION
// ============================================================================

/**
 * Start monitoring catch-up progress.
 *
 * Called when the preferred replica comes online but is not yet leader.
 * Launches a background thread that:
 * 1. Monitors commitIndex vs leader_last_commit_index_
 * 2. When caught up, triggers an election
 * 3. Exits when we become leader or monitoring is stopped
 */
// ============================================================================
// LEADERSHIP TRANSFER PROTOCOL
// ============================================================================

/**
 * Check if we should transfer leadership to preferred replica.
 *
 * Called by non-preferred leaders to determine if transfer conditions are met.
 *
 * Conditions for transfer:
 * 1. I am currently the leader
 * 2. I am NOT the preferred leader
 * 3. Preferred replica exists in the cluster
 * 4. Preferred replica is alive (responding to heartbeats)
 * 5. Preferred replica is caught up (match_index >= commit_index)
 *
 * @return true if leadership should be transferred
 */
bool RaftServer::ShouldTransferLeadership() {
  std::lock_guard<std::recursive_mutex> lock(mtx_);

  // Must be leader
  if (!is_leader_) {
    return false;
  }

  // Must not be preferred (preferred leaders don't transfer)
  if (AmIPreferredLeader()) {
    return false;
  }

  // Must have a preferred leader configured
  if (preferred_leader_site_id_ == INVALID_SITEID) {
    return false;
  }

  // Already transferring
  if (transferring_leadership_) {
    return false;
  }

  // Check if preferred replica is in our peer list
  auto it = match_index_.find(preferred_leader_site_id_);
  if (it == match_index_.end()) {
    Log_debug("[LEADERSHIP-TRANSFER] Site %d: Preferred replica %d not in peer list",
              site_id_, preferred_leader_site_id_);
    return false;
  }

  // Check if preferred replica is caught up
  slotid_t preferred_match_index = it->second;
  bool is_caught_up = (preferred_match_index >= commitIndex);

  if (!is_caught_up) {
    Log_debug("[LEADERSHIP-TRANSFER] Site %d: Preferred replica %d not caught up (match=%lu, commit=%lu)",
              site_id_, preferred_leader_site_id_, preferred_match_index, commitIndex);
    return false;
  }

  Log_info("[LEADERSHIP-TRANSFER] Site %d: Preferred replica %d is caught up! Ready to transfer",
           site_id_, preferred_leader_site_id_);
  return true;
}

/**
 * Initiate leadership transfer to preferred replica.
 *
 * Protocol with TimeoutNow:
 * 1. Stop accepting new commands (set transferring_leadership_)
 * 2. Ensure preferred replica has all committed entries (already checked)
 * 3. Send TimeoutNow RPC to preferred replica
 * 4. Wait for TimeoutNow response (with timeout)
 * 5. Step down from leadership
 * 6. Preferred replica starts election immediately and wins
 *
 * Edge Cases Handled:
 * - TimeoutNow RPC fails (network) → Step down anyway, fallback to standard election
 * - TimeoutNow succeeds but preferred can't start election → Still safe, standard election
 * - Timeout waiting for response → Step down anyway
 *
 * Safety: This maintains Raft safety because:
 * - We only transfer when preferred has all committed entries
 * - TimeoutNow just triggers timing, not outcome (voting still required)
 * - If TimeoutNow fails, standard Raft election happens
 */
void RaftServer::InitiateLeadershipTransfer() {
  // Check if server is shutting down
  if (stop_) {
    Log_info("[LEADERSHIP-TRANSFER] Site %d: Aborting transfer - server shutting down", site_id_);
    return;
  }

  siteid_t target_site_id;
  parid_t par_id;
  uint64_t current_term_snapshot;

  // ============================================================================
  // PIGGYBACKED LEADERSHIP TRANSFER (Approach 2)
  // ============================================================================

  {
    std::lock_guard<std::recursive_mutex> lock(mtx_);

    target_site_id = preferred_leader_site_id_;
    par_id = partition_id_;
    current_term_snapshot = currentTerm;

    // Mark transfer as in progress - this will suppress elections on non-preferred replicas
    transferring_leadership_ = true;
    leadership_transfer_start_time_ = Time::now();

    Log_info("[LEADERSHIP-TRANSFER] Site %d (partition %d): Starting transfer to site %d",
             site_id_, partition_id_, target_site_id);

    // Send heartbeats to ALL replicas
    for (auto& kv : match_index_) {
      siteid_t peer_site_id = kv.first;

      if (peer_site_id == site_id_) {
        continue;
      }

      slotid_t slot = commitIndex;
      ballot_t ballot = 0;
      uint64_t prevLogIndex = next_index_[peer_site_id] - 1;
      uint64_t prevLogTerm = 0;

      if (prevLogIndex > 0 && prevLogIndex < logs_.size()) {
        prevLogTerm = logs_[prevLogIndex]->term;
      }

      // Send trigger_election_now=true to ALL replicas during transfer:
      // - Preferred replica: Will start election
      // - Non-preferred replicas: Will activate election suppression
      bool trigger_election = true;  // Signal transfer to ALL replicas

      commo()->SendAppendEntries(
        peer_site_id,
        partition_id_,
        slot,
        ballot,
        true,
        site_id_,
        currentTerm,
        prevLogIndex,
        prevLogTerm,
        commitIndex,
        nullptr,
        0,
        trigger_election
      );
    }
  }

  // Sleep briefly to ensure the RPC library has time to send the packets.
  // Note: The preferred replica will wait 30ms before starting election,
  // so this sleep is just to ensure packet transmission, not to delay step-down.
  // We will likely step down earlier when we receive RequestVote from preferred replica.
  std::this_thread::sleep_for(std::chrono::milliseconds(20));

  // ============================================================================
  // Step Down from Leadership Immediately
  // ============================================================================
  // With piggybacked approach, we step down immediately after sending the message.
  // The preferred replica will:
  // 1. Reset its election timeout (from the heartbeat)
  // 2. Start election immediately (from the trigger_election_now flag)
  // 3. Win the election (since it's caught up and has all committed entries)
  //
  // Other replicas will:
  // 1. Reset their election timeouts (from normal heartbeats)
  // 2. Not start elections (timers reset)
  // 3. Vote for preferred replica when it requests votes
  {
    std::lock_guard<std::recursive_mutex> lock(mtx_);

    Log_info("[LEADERSHIP-TRANSFER] Site %d: Stepping down from leadership (current_term=%lu)",
             site_id_, currentTerm);

    // Become follower - this stops heartbeats and allows new leader to emerge
    setIsLeader(false);

    Log_info("[LEADERSHIP-TRANSFER] Site %d: Leadership transfer complete - now follower",
             site_id_);
  }
}

/**
 * Start background monitoring thread for leadership transfer.
 *
 * This thread runs on non-preferred leaders and periodically checks if
 * the preferred replica is caught up and ready to take over leadership.
 *
 * Called when:
 * - A non-preferred replica becomes leader
 * - Preferred leader is set while we're already leader
 */
void RaftServer::StartLeadershipTransferMonitoring() {
  if (leadership_monitor_stop_.load()) {
    leadership_monitor_stop_ = false;
  }

  // Stop any existing monitor thread
  if (leadership_monitor_thread_.joinable()) {
    leadership_monitor_stop_ = true;
    leadership_monitor_thread_.join();
    leadership_monitor_stop_ = false;
  }

  Log_info("[LEADERSHIP-TRANSFER] Site %d: Starting leadership transfer monitoring thread",
           site_id_);

  // Launch monitoring thread
  leadership_monitor_thread_ = std::thread([this]() {
    const uint64_t CHECK_INTERVAL_MS = 1000;  // Check every 1 second
    const uint64_t MIN_STABLE_TIME_US = 500000; // Wait 0.5 seconds (in microseconds) after becoming leader before transferring

    uint64_t became_leader_time = Time::now();

    Log_info("[LEADERSHIP-TRANSFER] Site %d: Monitor thread started (will check every %lums)",
             site_id_, CHECK_INTERVAL_MS);

    while (true) {
      std::this_thread::sleep_for(std::chrono::milliseconds(CHECK_INTERVAL_MS));

      bool should_transfer = false;

      // Critical section: check shared state with proper locking
      {
        std::lock_guard<std::recursive_mutex> lock(mtx_);

        // Check if we should stop monitoring
        if (leadership_monitor_stop_) {
          Log_info("[LEADERSHIP-TRANSFER] Site %d: Monitor stop requested, exiting", site_id_);
          break;
        }

        // Check if server is shutting down
        if (stop_) {
          Log_info("[LEADERSHIP-TRANSFER] Site %d: Server shutting down, exiting monitor", site_id_);
          break;
        }

        // Check if we're still leader
        if (!is_leader_) {
          Log_info("[LEADERSHIP-TRANSFER] Site %d: No longer leader, exiting monitor", site_id_);
          break;
        }

        // Check if we became preferred (no longer need to transfer)
        if (AmIPreferredLeader()) {
          Log_info("[LEADERSHIP-TRANSFER] Site %d: I am now preferred leader, exiting monitor",
                   site_id_);
          break;
        }

        // Wait for cluster to stabilize after becoming leader
        uint64_t time_as_leader = Time::now() - became_leader_time;
        if (time_as_leader < MIN_STABLE_TIME_US) {
          continue;
        }

        // Check if we should transfer leadership
        if (ShouldTransferLeadership()) {
          Log_info("[LEADERSHIP-TRANSFER] Site %d: Conditions met, initiating transfer NOW",
                   site_id_);
          should_transfer = true;
        }
      } // End critical section - LOCK RELEASED

      // Call InitiateLeadershipTransfer WITHOUT holding lock to avoid deadlock
      if (should_transfer) {
        InitiateLeadershipTransfer();
        break;  // Exit after transferring
      }
    }

    Log_info("[LEADERSHIP-TRANSFER] Site %d: Monitor thread exiting", site_id_);
  });
}

/**
 * Stop leadership transfer monitoring thread.
 *
 * Called when:
 * - We lose leadership
 * - We become the preferred leader
 * - Server is shutting down
 */
void RaftServer::StopLeadershipTransferMonitoring() {
  leadership_monitor_stop_ = true;

  // Detach the monitor thread so it can exit gracefully without deadlock
  // The thread will see leadership_monitor_stop_ and exit on its own
  if (leadership_monitor_thread_.joinable()) {
    Log_debug("[LEADERSHIP-TRANSFER] Site %d: Detaching monitor thread (will exit on its own)", site_id_);
    leadership_monitor_thread_.detach();
  }
}

} // namespace janus
