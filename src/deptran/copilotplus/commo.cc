#include "../__dep__.h"
#include "../constants.h"
#include "commo.h"

// #define SKIP

namespace janus {

void CopilotPlusFastAcceptQuorumEvent::FeedResponse(bool y, bool ok) {
  if (y) {
    VoteYes();
    if (ok)
      n_fastac_ok_++;
    else
      n_fastac_reply_++;
  } else {
    VoteNo();
  }
}

void CopilotPlusFastAcceptQuorumEvent::FeedRetDep(uint64_t dep) {
  verify(ret_deps_.size() < n_total_);
  ret_deps_.push_back(dep);
}

uint64_t CopilotPlusFastAcceptQuorumEvent::GetFinalDep() {
  verify(ret_deps_.size() >= n_total_ / 2 + 1);
  std::sort(ret_deps_.begin(), ret_deps_.end());
  return ret_deps_[n_total_ / 2];
}

bool CopilotPlusFastAcceptQuorumEvent::FastYes() {
  return n_fastac_ok_ >= CopilotPlusCommo::CurpFastQuorumSize(n_total_);
}

bool CopilotPlusFastAcceptQuorumEvent::FastNo() {
  return Yes() && !FastYes();
}


inline void CopilotPlusPrepareQuorumEvent::FeedRetCmd(ballot_t ballot,
                                                  uint64_t dep,
                                                  uint8_t is_pilot, slotid_t slot,
                                                  shared_ptr<Marshallable> cmd,
                                                  enum Status status) {
  uint32_t int_status = GET_STATUS(static_cast<uint32_t>(status));
  // int_status &= CLR_FLAG_TAKEOVER;
  verify(int_status <= n_status);
  if (int_status >= Status::COMMITED) { // committed or executed
    committed_seen_ = true;
    int_status = Status::COMMITED;  // reduce all status greater than COMMIT to COMMIT
  } else if (int_status == Status::FAST_ACCEPTED) {
    int_status = Status::FAST_ACCEPTED_EQ; // reduce FAST_ACCEPTED to FAST_ACCEPTED_EQ
  }
  ret_cmds_by_status_[int_status].emplace_back(CopilotPlusData{cmd, dep, is_pilot, slot, ballot, int_status, 0, 0});
}

inline size_t CopilotPlusPrepareQuorumEvent::GetCount(enum Status status) {
  return ret_cmds_by_status_[status].size();
}

vector<CopilotPlusData>& CopilotPlusPrepareQuorumEvent::GetCmds(enum Status status) {
  return ret_cmds_by_status_[status];
}

bool CopilotPlusPrepareQuorumEvent::IsReady() {
  if (timeouted_) {
    // TODO add time out support
    return true;
  }
  if (committed_seen_) {
    return true;
  }
  if (Yes()) {
    //      Log_info("voted: %d is equal or greater than quorum: %d",
    //                (int)n_voted_yes_, (int) quorum_);
    // ready_time = std::chrono::steady_clock::now();
    return true;
  } else if (No()) {
    return true;
  }
  //    Log_debug("voted: %d is smaller than quorum: %d",
  //              (int)n_voted_, (int) quorum_);
  return false;
}

void CopilotPlusPrepareQuorumEvent::Show() {
  std::cout << committed_seen_ << std::endl;
  for (int i = 0; i < ret_cmds_by_status_.size(); i++)
    std::cout << i << ":" << ret_cmds_by_status_[i].size() << std::endl;
}


CopilotPlusCommo::CopilotPlusCommo(PollMgr *poll) : Communicator(poll) {}

// shared_ptr<CopilotPlusForwardQuorumEvent>
// CopilotPlusCommo::ForwardResultToCoordinator(parid_t par_id,
//                                               shared_ptr<Marshallable>& cmd,
//                                               Position pos,
//                                               bool_t accepted) {
//   int n = Config::GetConfig()->GetPartitionSize(par_id);
//   auto e = Reactor::CreateSpEvent<CopilotPlusForwardQuorumEvent>(1, 1);
//   auto proxies = rpc_par_proxies_[par_id];
//   for (auto& p : proxies) {
//     auto proxy = (CopilotPlusProxy *)p.second;
//     auto site = p.first;
//     // TODO: generalize
//     if (0 == site) {
//       FutureAttr fuattr;
//       fuattr.callback = [e](Future *fu) {
//         // TODO
//         // MarshallDeputy md;

//         // slotid_t sgs_i_y, sgs_i_n, sgs_j_y, sgs_j_n; // sgs -> suggested
//         // ballot_t b;
//         // fu->get_reply() >> accepted >> sgs_i_y >> sgs_i_n >> sgs_j_y >> sgs_j_n >> b;
//         // e->FeedResponse(accepted >> sgs_i_y >> sgs_i_n >> sgs_j_y >> sgs_j_n);
//       };

//       // Future *f = proxy->async_Forward(accepted, i_y, i_n, j_y, j_n, ballot, fuattr);
//       // Future::safe_release(f);
//     }
//   }
//   return nullptr;
// }

shared_ptr<CopilotPlusPrepareQuorumEvent>
CopilotPlusCommo::BroadcastPrepare(parid_t par_id,
                               uint8_t is_pilot,
                               slotid_t slot_id,
                               ballot_t ballot) {
  int n = Config::GetConfig()->GetPartitionSize(par_id);
  auto e = Reactor::CreateSpEvent<CopilotPlusPrepareQuorumEvent>(n, CurpQuorumSize(n));
  auto proxies = rpc_par_proxies_[par_id];
  struct DepId di;

  // WAN_WAIT
  for (auto& p : proxies) {
    auto proxy = (CopilotPlusProxy *)p.second;
    auto site = p.first;

    FutureAttr fuattr;
    fuattr.callback = [e, ballot, is_pilot, slot_id, site](Future *fu) {
      MarshallDeputy md;
      ballot_t b;
      uint64_t dep;
      status_t status;

      fu->get_reply() >> md >> b >> dep >> status;
      bool ok = (ballot == b);
      
      if (ok) {
        e->FeedRetCmd(ballot,
                      dep,
                      is_pilot, slot_id,
                      const_cast<MarshallDeputy&>(md).sp_data_,
                      static_cast<enum Status>(status));
      } // Feed command before feeding response, since if there is a committed command,
        // the prepare event will be ready in advance without waiting for a quorum.
      e->FeedResponse(ok);

      e->RemoveXid(site);
    };

    Future *f = proxy->async_Prepare(is_pilot, slot_id, ballot, di, fuattr);
    e->AddXid(site, f->get_xid());
    Future::safe_release(f);
  }

  return e;
}

shared_ptr<CopilotPlusFastAcceptQuorumEvent>
CopilotPlusCommo::BroadcastFastAccept(parid_t par_id,
                                  uint8_t is_pilot,
                                  slotid_t slot_id,
                                  ballot_t ballot,
                                  uint64_t dep,
                                  shared_ptr<Marshallable> cmd) {
  Log_info("[copilot+] [BroadcastFastAccept+]");
  int n = Config::GetConfig()->GetPartitionSize(par_id);
  auto e = Reactor::CreateSpEvent<CopilotPlusFastAcceptQuorumEvent>(n, CurpFastQuorumSize(n));
  auto proxies = rpc_par_proxies_[par_id];
  struct DepId di;

  // WAN_WAIT
  for (auto& p : proxies) {
    auto proxy = (CopilotPlusProxy *)p.second;
    auto site = p.first;

#ifdef SKIP
    if (site == 1) continue;
#endif
    if (1==0/*site == loc_id_*/) { //[TODO: recover it to site == loc_id_]
      ballot_t b;
      slotid_t sgst_dep;
      static_cast<CopilotPlusServer *>(rep_sched_)->OnFastAccept(
        is_pilot, slot_id, ballot, dep, cmd, di, &b, &sgst_dep, nullptr);
      e->FeedResponse(true, true);
      e->FeedRetDep(dep);
    } else {
      FutureAttr fuattr;
      fuattr.callback = [e, dep, ballot, site](Future *fu) {
        ballot_t b;
        slotid_t sgst_dep;
        fu->get_reply() >> b >> sgst_dep;
        bool ok = (ballot == b);
        e->FeedResponse(ok, sgst_dep == dep);
        if (ok) {
          e->FeedRetDep(sgst_dep);
        }

        e->RemoveXid(site);
      };

      verify(cmd);
      MarshallDeputy md(cmd);
      Log_info("[copilot+] [BroadcastFastAccept in] before async_FastAccept");
      Future *f = proxy->async_FastAccept(is_pilot, slot_id, ballot, dep, md, di, fuattr);
      e->AddXid(site, f->get_xid());
      Future::safe_release(f);
      Log_info("[copilot+] [BroadcastFastAccept in] after async_FastAccept");
    }
  }
  Log_info("[copilot+] [BroadcastFastAccept-]");
  return e;
}

shared_ptr<CopilotAcceptQuorumEvent>
CopilotPlusCommo::BroadcastAccept(parid_t par_id,
                              uint8_t is_pilot,
                              slotid_t slot_id,
                              ballot_t ballot,
                              uint64_t dep,
                              shared_ptr<Marshallable> cmd) {
  int n = Config::GetConfig()->GetPartitionSize(par_id);
  auto e = Reactor::CreateSpEvent<CopilotAcceptQuorumEvent>(n, CurpQuorumSize(n));
  auto proxies = rpc_par_proxies_[par_id];
  struct DepId di;

  // WAN_WAIT
  for (auto& p : proxies) {
    auto proxy = (CopilotPlusProxy *)p.second;
    auto site = p.first;

#ifdef SKIP
    if (site == 1) continue;
#endif
    if (site == loc_id_) {
      ballot_t b;
      static_cast<CopilotPlusServer *>(rep_sched_)->OnAccept(
        is_pilot, slot_id, ballot, dep, cmd, di, &b, nullptr);
      e->FeedResponse(true);
    } else {
      FutureAttr fuattr;
      fuattr.callback = [e, ballot, site](Future *fu) {
        ballot_t b;
        fu->get_reply() >> b;
        e->FeedResponse(ballot == b);

        e->RemoveXid(site);
      };

      MarshallDeputy md(cmd);
      Future *f = proxy->async_Accept(is_pilot, slot_id, ballot, dep, md, di, fuattr);
      e->AddXid(site, f->get_xid());
      Future::safe_release(f);
    }
  }

  return e;
}

shared_ptr<CopilotFakeQuorumEvent>
CopilotPlusCommo::BroadcastCommit(parid_t par_id,
                                   uint8_t is_pilot,
                                   slotid_t slot_id,
                                   uint64_t dep,
                                   shared_ptr<Marshallable> cmd) {
  int n = Config::GetConfig()->GetPartitionSize(par_id);
  auto e = Reactor::CreateSpEvent<CopilotFakeQuorumEvent>(n);
  auto proxies = rpc_par_proxies_[par_id];
  
  for (auto& p : proxies) {
    auto proxy = (CopilotPlusProxy*) p.second;
    auto site = p.first;

#ifdef SKIP
    if (site == 1) continue;
#endif
    FutureAttr fuattr;
    fuattr.callback = [e, site](Future* fu) {
      e->RemoveXid(site);
    };
    MarshallDeputy md(cmd);
    Future *f = proxy->async_Commit(is_pilot, slot_id, dep, md, fuattr);
    e->AddXid(site, f->get_xid());
    Future::safe_release(f);
  }

  return e;
}

inline int CopilotPlusCommo::maxFailure(int total) {
  return (total + 1) / 2 - 1;
}

inline int CopilotPlusCommo::CurpFastQuorumSize(int total) {
  int max_fail = maxFailure(total);
  return max_fail + (max_fail + 1) / 2 + 1;
}

inline int CopilotPlusCommo::CurpQuorumSize(int total) {
  return total - maxFailure(total);
}

} // namespace janus
