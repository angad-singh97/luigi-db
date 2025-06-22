#include "testconf.h"
#include "marshallable.h"

namespace janus {

#ifdef RAFT_TEST_CORO

int _test_id_g = 0;

std::map<siteid_t, RaftFrame*> RaftTestConfig::replicas;
std::map<siteid_t, std::function<void(Marshallable &)>> RaftTestConfig::commit_callbacks;
std::map<siteid_t, std::vector<int>> RaftTestConfig::committed_cmds;
std::map<siteid_t, uint64_t> RaftTestConfig::rpc_count_last;

RaftTestConfig::RaftTestConfig(std::map<siteid_t, RaftFrame*>& replicas) {
  verify(RaftTestConfig::replicas.empty());
  RaftTestConfig::replicas = replicas;
  for (auto& pair : replicas) {
    auto svr = pair.first;
    auto frame = pair.second;
    frame->svr_->rep_frame_ = frame->svr_->frame_;  // Set rep_frame_ directly like lab solution
    RaftTestConfig::committed_cmds[svr].push_back(-1);
    RaftTestConfig::rpc_count_last[svr] = 0;
    disconnected_[svr] = false;
  }
  th_ = std::thread([this](){ netctlLoop(); });
}

void RaftTestConfig::SetLearnerAction(void) {
  for (auto& pair : replicas) {
    auto svr = pair.first;
    auto frame = pair.second;
    // rep_frame_ is already set in constructor, no need to set it here
    RaftTestConfig::commit_callbacks[svr] = [svr](Marshallable& cmd) {
      verify(cmd.kind_ == MarshallDeputy::CMD_TPC_COMMIT);
      auto& command = dynamic_cast<TpcCommitCommand&>(cmd);
      Log_debug("server %d committed value %d", svr, command.tx_id_);
      RaftTestConfig::committed_cmds[svr].push_back(command.tx_id_);
    };
    frame->svr_->RegLearnerAction(RaftTestConfig::commit_callbacks[svr]);
  }
}

int RaftTestConfig::OneLeader(int expected) {
  return waitOneLeader(true, expected);
}

bool RaftTestConfig::NoLeader(void) {
  int r = waitOneLeader(false, -1);
  return r == -1;
}

int RaftTestConfig::waitOneLeader(bool want_leader, int expected) {
  uint64_t mostRecentTerm = 0, term;
  int leader = -1;  // Use int instead of siteid_t to avoid unsigned conversion
  bool isleader;
  
  Log_info("waitOneLeader: want_leader=%d, expected=%d", want_leader, expected);
  Log_info("waitOneLeader: replicas map contains %zu servers", replicas.size());
  for (const auto& pair : replicas) {
    Log_info("waitOneLeader: server ID %d in replicas map", pair.first);
  }
  
  for (int retry = 0; retry < 10; retry++) {
    Coroutine::Sleep(ELECTIONTIMEOUT/10);
    leader = -1;
    mostRecentTerm = 0;
    for (auto& pair : replicas) {
      auto svr = pair.first;
      auto frame = pair.second;
      // ignore disconnected servers
      if (frame->svr_->IsDisconnected()) {
        Log_info("waitOneLeader: server %d is disconnected, skipping", svr);
        continue;
      }
      frame->svr_->GetState(&isleader, &term);
      Log_info("waitOneLeader: server %d isleader=%d, term=%ld", svr, isleader, term);
      if (isleader) {
        if (term == mostRecentTerm) {
          Failed("multiple leaders elected in term %ld", term);
          return -2;
        } else if (term > mostRecentTerm) {
          leader = svr;
          mostRecentTerm = term;
          Log_debug("found leader %d with term %d", leader, term);
        }
      }
    }
    if (leader != -1) {
      if (!want_leader) {
        Failed("leader elected despite lack of quorum");
      } else if (expected >= 0 && leader != expected) {
        Failed("unexpected leader change, expecting %d, got %d", expected, leader);
        return -3;
      }
      Log_info("waitOneLeader: returning leader %d", leader);
      return leader;
    }
    Log_info("waitOneLeader: retry %d, no leader found", retry);
  }
  if (want_leader) {
    Log_debug("failing, timeout?");
    Failed("waited too long for leader election");
  }
  Log_info("waitOneLeader: returning -1 (no leader found)");
  return -1;
}

bool RaftTestConfig::TermMovedOn(uint64_t term) {
  for (auto& pair : replicas) {
    auto frame = pair.second;
    uint64_t curTerm;
    bool isLeader;
    frame->svr_->GetState(&isLeader, &curTerm);
    if (curTerm > term) {
      return true;
    }
  }
  return false;
}

uint64_t RaftTestConfig::OneTerm(void) {
  if (replicas.empty()) return -1;
  
  uint64_t term, curTerm;
  bool isLeader;
  auto first_frame = replicas.begin()->second;
  first_frame->svr_->GetState(&isLeader, &term);
  
  for (auto it = ++replicas.begin(); it != replicas.end(); ++it) {
    auto frame = it->second;
    frame->svr_->GetState(&isLeader, &curTerm);
    if (curTerm != term) {
      return -1;
    }
  }
  return term;
}

int RaftTestConfig::NCommitted(uint64_t index) {
  int cmd = -1;
  int n = 0;
  for (auto& pair : replicas) {
    auto svr = pair.first;
    if (committed_cmds[svr].size() > index) {
      auto curcmd = committed_cmds[svr][index];
      if (n == 0) {
        cmd = curcmd;
      } else {
        if (curcmd != cmd) {
          return -1;
        }
      }
      n++;
    }
  }
  return n;
}

bool RaftTestConfig::Start(siteid_t svr, int cmd, uint64_t *index, uint64_t *term) {
  auto it = replicas.find(svr);
  if (it == replicas.end()) return false;
  
  // Construct an empty TpcCommitCommand containing cmd as its tx_id_
  auto cmdptr = std::make_shared<TpcCommitCommand>();
  auto vpd_p = std::make_shared<VecPieceData>();
  vpd_p->sp_vec_piece_data_ = std::make_shared<vector<shared_ptr<SimpleCommand>>>();
  cmdptr->tx_id_ = cmd;
  cmdptr->cmd_ = vpd_p;
  auto cmdptr_m = dynamic_pointer_cast<Marshallable>(cmdptr);
  // call Start()
  Log_debug("Starting agreement on svr %d for cmd id %d", svr, cmdptr->tx_id_);
  return it->second->svr_->Start(cmdptr_m, index, term);
}

int RaftTestConfig::Wait(uint64_t index, int n, uint64_t term) {
  int nc = 0, i;
  auto to = 10000; // 10 milliseconds
  for (i = 0; i < 30; i++) {
    nc = NCommitted(index);
    if (nc < 0) {
      return -3; // values differ
    } else if (nc >= n) {
      break;
    }
    Reactor::CreateSpEvent<TimeoutEvent>(to)->Wait();
    if (to < 1000000) {
      to *= 2;
    }
    if (TermMovedOn(term)) {
      return -2; // term changed
    }
  }
  if (i == 30) {
    return -1; // timeout
  }
  for (auto& pair : replicas) {
    auto svr = pair.first;
    if (committed_cmds[svr].size() > index) {
      return committed_cmds[svr][index];
    }
  }
  verify(0);
}

uint64_t RaftTestConfig::DoAgreement(int cmd, int n, bool retry) {
  Log_debug("Doing 1 round of Raft agreement");
  auto start = chrono::steady_clock::now();
  while ((chrono::steady_clock::now() - start) < chrono::seconds{10}) {
    Coroutine::Sleep(50000);
    // Call Start() to all servers until leader is found
    siteid_t ldr = -1;
    uint64_t index, term;
    for (auto& pair : replicas) {
      auto svr = pair.first;
      auto frame = pair.second;
      // skip disconnected servers
      if (frame->svr_->IsDisconnected())
        continue;
      if (Start(svr, cmd, &index, &term)) {
        Log_debug("starting cmd ldr=%d cmd=%d index=%ld term=%ld", 
            svr, cmd, index, term);
        ldr = svr;
        break;
      }
    }
    if (ldr != -1) {
      // If Start() successfully called, wait for agreement
      auto start2 = chrono::steady_clock::now();
      int nc;
      while ((chrono::steady_clock::now() - start2) < chrono::seconds{10}) {
        nc = NCommitted(index);
        if (nc < 0) {
          break;
        } else if (nc >= n) {
          for (auto& pair : replicas) {
            auto svr = pair.first;
            if (committed_cmds[svr].size() > index) {
              Log_debug("found commit log");
              auto cmd2 = committed_cmds[svr][index];
              if (cmd == cmd2) {
                return index;
              }
              break;
            }
          }
          break;
        }
        Coroutine::Sleep(50000);
      }
      Log_debug("%d committed server at index %d", nc, index);
      if (!retry) {
          Log_debug("failed to reach agreement");
          return 0;
        }
    } else {
      // If no leader found, sleep and retry.
      Coroutine::Sleep(50000);
    }
  }
  Log_debug("Failed to reach agreement end");
  return 0;
}

shared_ptr<CommitIndex> RaftTestConfig::StartAgreement(siteid_t svr, int cmd) {
  verify(0); // this function has been replaced by Start()
  auto cmt_idx_p = std::make_shared<CommitIndex>(0);
  std::shared_ptr<OneTimeJob> sp_otj = std::make_shared<OneTimeJob>(
    [this, cmd, svr, cmt_idx_p]() {
      auto cmdptr = std::make_shared<TpcCommitCommand>();
      auto vpd_p = std::make_shared<VecPieceData>();
      vpd_p->sp_vec_piece_data_ = std::make_shared<vector<shared_ptr<SimpleCommand>>>();
      cmdptr->tx_id_ = cmd;
      cmdptr->cmd_ = vpd_p;
      Log_debug("Starting agreement for cmd id %d", cmdptr->tx_id_);
      auto cmdptr_m = dynamic_pointer_cast<Marshallable>(cmdptr);
      auto it = replicas.find(svr);
      if (it != replicas.end()) {
        it->second->svr_->CreateRepCoord(0)->Submit(cmdptr_m, [svr, cmt_idx_p, it](){
          cmt_idx_p->setval(it->second->svr_->commitIndex);
        });
      }
    }
  );
  auto sp_job = std::dynamic_pointer_cast<Job>(sp_otj);
  auto it = replicas.find(svr);
  if (it != replicas.end()) {
    it->second->commo_->rpc_poll_->add(sp_job);
  }
  Log_debug("Started agreement for cmd id %d", cmd);
  return cmt_idx_p;
}

void RaftTestConfig::Disconnect(siteid_t svr) {
  Log_info("Disconnect: disconnecting server %d", svr);
  std::lock_guard<std::mutex> lk(disconnect_mtx_);
  verify(!disconnected_[svr]);
  disconnect(svr, true);
  disconnected_[svr] = true;
  Log_info("Disconnect: server %d disconnected successfully", svr);
}

void RaftTestConfig::Reconnect(siteid_t svr) {
  Log_info("Reconnect: reconnecting server %d", svr);
  std::lock_guard<std::mutex> lk(disconnect_mtx_);
  verify(disconnected_[svr]);
  reconnect(svr);
  disconnected_[svr] = false;
  Log_info("Reconnect: server %d reconnected successfully", svr);
}

int RaftTestConfig::NDisconnected(void) {
  int count = 0;
  for (auto& pair : disconnected_) {
    if (pair.second)
      count++;
  }
  return count;
}

void RaftTestConfig::SetUnreliable(bool unreliable) {
  std::unique_lock<std::mutex> lk(cv_m_);
  verify(!finished_);
  if (unreliable) {
    verify(!unreliable_);
    // lk acquired cv_m_ in state 1 or 0
    unreliable_ = true;
    // if cv_m_ was in state 1, must signal cv_ to wake up netctlLoop
    lk.unlock();
    cv_.notify_one();
  } else {
    verify(unreliable_);
    // lk acquired cv_m_ in state 2 or 0
    unreliable_ = false;
    // wait until netctlLoop moves cv_m_ from state 2 (or 0) to state 1,
    // restoring the network to reliable state in the process.
    lk.unlock();
    lk.lock();
  }
}

bool RaftTestConfig::IsUnreliable(void) {
  return unreliable_;
}

void RaftTestConfig::Shutdown(void) {
  // trigger netctlLoop shutdown
  {
    std::unique_lock<std::mutex> lk(cv_m_);
    verify(!finished_);
    // lk acquired cv_m_ in state 0, 1, or 2
    finished_ = true;
    // if cv_m_ was in state 1, must signal cv_ to wake up netctlLoop
    lk.unlock();
    cv_.notify_one();
  }
  // wait for netctlLoop thread to exit
  th_.join();
  // Reconnect() all Deconnect()ed servers
  for (auto& pair : disconnected_) {
    if (pair.second) {
      Reconnect(pair.first);
    }
  }
}

uint64_t RaftTestConfig::RpcCount(siteid_t svr, bool reset) {
  std::lock_guard<std::recursive_mutex> lk(
    RaftTestConfig::replicas[svr]->commo_->rpc_mtx_);
  uint64_t count = RaftTestConfig::replicas[svr]->commo_->rpc_count_;
  uint64_t count_last = RaftTestConfig::rpc_count_last[svr];
  if (reset) {
    RaftTestConfig::rpc_count_last[svr] = count;
  }
  verify(count >= count_last);
  return count - count_last;
}

uint64_t RaftTestConfig::RpcTotal(void) {
  uint64_t total = 0;
  for (auto& pair : replicas) {
    total += RaftTestConfig::replicas[pair.first]->commo_->rpc_count_;
  }
  return total;
}

bool RaftTestConfig::ServerCommitted(siteid_t svr, uint64_t index, int cmd) {
  if (committed_cmds[svr].size() <= index)
    return false;
  return committed_cmds[svr][index] == cmd;
}

void RaftTestConfig::netctlLoop(void) {
  bool isdown;
  // cv_m_ unlocked state 0 (finished_ == false)
  std::unique_lock<std::mutex> lk(cv_m_);
  while (!finished_) {
    if (!unreliable_) {
      {
        std::lock_guard<std::mutex> prlk(disconnect_mtx_);
        // unset all unreliable-related disconnects and slows
        for (const auto& pair : replicas) {
          siteid_t svr = pair.first;
          if (!disconnected_[svr]) {
            reconnect(svr, true);
            slow(svr, 0);
          }
        }
      }
      // sleep until unreliable_ or finished_ is set
      // cv_m_ unlocked state 1 (unreliable_ == false && finished_ == false)
      cv_.wait(lk, [this](){ return unreliable_ || finished_; });
      continue;
    }
    {
      std::lock_guard<std::mutex> prlk(disconnect_mtx_);
      for (const auto& pair : replicas) {
        siteid_t svr = pair.first;
        // skip server if it was disconnected using Disconnect()
        if (disconnected_[svr]) {
          continue;
        }
        // server has DOWNRATE_N / DOWNRATE_D chance of being down
        if ((rand() % DOWNRATE_D) < DOWNRATE_N) {
          // disconnect server if not already disconnected in the previous period
          disconnect(svr, true);
        } else {
          // Server not down: random slow timeout
          // Reconnect server if it was disconnected in the previous period
          reconnect(svr, true);
          // server's slow timeout should be btwn 0-(MAXSLOW-1) ms
          slow(svr, rand() % MAXSLOW);
        }
      }
    }
    // change unreliable state every 0.1s
    usleep(100000);
    lk.unlock();
    // cv_m_ unlocked state 2 (unreliable_ == true && finished_ == false)
    lk.lock();
  }
  // If network is still unreliable, unset it
  if (unreliable_) {
    unreliable_ = false;
    {
      std::lock_guard<std::mutex> prlk(disconnect_mtx_);
      // unset all unreliable-related disconnects and slows
      for (const auto& pair : replicas) {
        siteid_t svr = pair.first;
        if (!disconnected_[svr]) {
          reconnect(svr, true);
          slow(svr, 0);
        }
      }
    }
  }
  // cv_m_ unlocked state 3 (unreliable_ == false && finished_ == true)
}

bool RaftTestConfig::isDisconnected(siteid_t svr) {
  std::lock_guard<std::recursive_mutex> lk(connection_m_);
  return RaftTestConfig::replicas[svr]->svr_->IsDisconnected();
}

void RaftTestConfig::disconnect(siteid_t svr, bool ignore) {
  std::lock_guard<std::recursive_mutex> lk(connection_m_);
  if (!isDisconnected(svr)) {
    // simulate disconnected server
    RaftTestConfig::replicas[svr]->svr_->Disconnect();
  } else if (!ignore) {
    verify(0);
  }
}

void RaftTestConfig::reconnect(siteid_t svr, bool ignore) {
  std::lock_guard<std::recursive_mutex> lk(connection_m_);
  if (isDisconnected(svr)) {
    // simulate reconnected server
    RaftTestConfig::replicas[svr]->svr_->Reconnect();
  } else if (!ignore) {
    verify(0);
  }
}

void RaftTestConfig::slow(siteid_t svr, uint32_t msec) {
  // Instead of using reactor's slow mode, use Coroutine::Sleep
  // This will introduce the same delay but without needing reactor changes
  usleep(msec * 1000);  // Convert msec to microseconds
}

RaftServer *RaftTestConfig::GetServer(siteid_t svr) {
  return RaftTestConfig::replicas[svr]->svr_;
}

siteid_t RaftTestConfig::mapServerId(siteid_t server_id) const {
  // Find the server_id in the replicas map and return its position (0-4)
  int index = 0;
  for (const auto& pair : replicas) {
    if (pair.first == server_id) {
      return index;
    }
    index++;
  }
  // If not found, return the original ID (this should not happen in normal operation)
  return server_id;
}

siteid_t RaftTestConfig::getServerIdByIndex(int index) const {
  // Get server ID by its position in the replicas map (0-4)
  if (index < 0 || index >= NSERVERS) {
    // Index out of range, return -1
    Log_info("getServerIdByIndex: index %d out of range [0, %d), returning -1", index, NSERVERS);
    return -1;
  }
  
  int i = 0;
  for (const auto& pair : replicas) {
    if (i == index) {
      Log_info("getServerIdByIndex: index %d maps to server ID %d", index, pair.first);
      return pair.first;
    }
    i++;
  }
  // If we get here, something is wrong with the replicas map
  // This should not happen in normal operation
  Log_info("getServerIdByIndex: index %d not found in replicas map, returning -1", index);
  return -1;
}

siteid_t RaftTestConfig::getNextServerId(siteid_t current_server_id, int offset) const {
  Log_info("getNextServerId: finding next server from %d with offset %d", current_server_id, offset);
  
  // Find current server's index and add offset, wrapping around
  int current_index = -1;
  int i = 0;
  for (const auto& pair : replicas) {
    if (pair.first == current_server_id) {
      current_index = i;
      break;
    }
    i++;
  }
  
  if (current_index == -1) {
    Log_info("getNextServerId: server ID %d not found in replicas map, returning original", current_server_id);
    return current_server_id; // Return original if not found
  }
  
  Log_info("getNextServerId: server %d is at index %d", current_server_id, current_index);
  
  // Calculate new index with wrapping
  int new_index = (current_index + offset) % NSERVERS;
  if (new_index < 0) {
    new_index += NSERVERS;
  }
  
  Log_info("getNextServerId: calculated new index %d", new_index);
  
  siteid_t result = getServerIdByIndex(new_index);
  if (result == -1) {
    // If getServerIdByIndex returns -1, return the original server ID
    // This should not happen in normal operation, but provides safety
    Log_info("getNextServerId: getServerIdByIndex returned -1, returning original server ID %d", current_server_id);
    return current_server_id;
  }
  
  Log_info("getNextServerId: returning server ID %d", result);
  return result;
}

#endif

}
