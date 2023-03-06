#include "../__dep__.h"
#include "commo.h"

namespace janus {

void CopilotPlusSubmitQuorumEvent::FeedResponse(bool_t accepted, slotid_t i, slotid_t j, ballot_t ballot) {
  Log_info("[copilot+] FeedResponse accepted=%d i=%d j=%d ballot=%d", accepted, i, j, ballot);
  response_received_++;
  responses_.push_back(ResponsePack{i, j, ballot});
}

bool CopilotPlusSubmitQuorumEvent::FastYes() {
  if (response_received_ < CopilotPlusCommo::fastQuorumSize(n_total_)) return false;
  Log_info("[copilot+] Quorum result: FastYes");
  int max_len = FindMax();
  return max_len >= CopilotPlusCommo::fastQuorumSize(n_total_);
}

bool CopilotPlusSubmitQuorumEvent::RecoverWithOpYes() {
  if (response_received_ < quorum_) return false;
  Log_info("[copilot+] Quorum result: RecoverWithOpYes");
  int max_len = FindMax();
  return max_len >= CopilotPlusCommo::smallQuorumSize(n_total_);
}

bool CopilotPlusSubmitQuorumEvent::RecoverWithoutOpYes() {
  if (response_received_ < quorum_) return false;
  Log_info("[copilot+] Quorum result: RecoverWithoutOpYes");
  int max_len = FindMax();
  return max_len < CopilotPlusCommo::smallQuorumSize(n_total_);
}

bool CopilotPlusSubmitQuorumEvent::IsReady() {
  Log_info("[copilot+] enter IsReady");
  if (timeouted_) {
    //Log_info("[copilot+] timeouted_ ready");
    return true;
  }
  if (FastYes()) {
    //Log_info("[copilot+] FastYes ready");
    return true;
  } else if (RecoverWithOpYes()) {
    //Log_info("[copilot+] RecoverWithOpYes ready");
    return true;
  } else if (RecoverWithoutOpYes()) {
    //Log_info("[copilot+] RecoverWithOpYes ready");
    return true;
  }
  return false;
}

CopilotPlusCommo::CopilotPlusCommo(PollMgr *poll): Communicator(poll) {

}

shared_ptr<CopilotPlusSubmitQuorumEvent>
CopilotPlusCommo::BroadcastSubmit(parid_t par_id,
                                  shared_ptr<Marshallable> cmd) {
  Log_info("[copilot+] enter BroadcastSubmit in svr=%d", svr_ == nullptr ? -1 : svr_->loc_id_);
  int n = Config::GetConfig()->GetPartitionSize(par_id);
  auto e = Reactor::CreateSpEvent<CopilotPlusSubmitQuorumEvent>(n, quorumSize(n));
  //Log_info("[copilot+] in BroadcastSubmit in svr=%d", svr_ == nullptr ? -1 : svr_->loc_id_);
  auto proxies = rpc_par_proxies_[par_id];
  for (auto& p : proxies) {
    auto proxy = (CopilotPlusProxy*) p.second;
    auto site = p.first;
    FutureAttr fuattr;
    fuattr.callback = [e](Future* fu) {
      bool_t accepted;
      slotid_t i, j;
      ballot_t ballot;
      fu->get_reply() >> accepted >> i >> j >> ballot;
      Log_info("[copilot+] get reply accepted=%d i=%d j=%d ballot=%d", accepted, i, j, ballot);
      e->FeedResponse(accepted, i, j, ballot);
    };
    MarshallDeputy md(cmd);
    //Log_info("[copilot+] in BroadcastSubmit in svr=%d", svr_ == nullptr ? -1 : svr_->loc_id_);
    Future *f = proxy->async_Submit(md, fuattr);
    //Log_info("[copilot+] in BroadcastSubmit in svr=%d, after async_Submit", svr_ == nullptr ? -1 : svr_->loc_id_);
    Future::safe_release(f);
  }
  Log_info("[copilot+] exit BroadcastSubmit in svr=%d", svr_ == nullptr ? -1 : svr_->loc_id_);
  return e;
}

void
CopilotPlusCommo::ForwardReply(parid_t par_id,
                               siteid_t site_id,
                               slotid_t i,
                               slotid_t j,
                               ballot_t ballot,
                               bool_t accepted) {
  // int n = Config::GetConfig()->GetPartitionSize(par_id);

  // auto proxies = rpc_par_proxies_[par_id];
  // for (auto& p : proxies) {
  //   auto proxy = (CopilotPlusProxy*) p.second;
  //   auto site = p.first;

  //   FutureAttr fuattr;
  //   fuattr.callback = [](Future* fu) {
  //   };
  //   Future *f = proxy->async_Submit(md, fuattr);
  //   Future::safe_release(f);
  // }
}

shared_ptr<CopilotPlusFrontRecoverQuorumEvent>
CopilotPlusCommo::BroadcastFrontRecover(parid_t par_id,
                                        shared_ptr<Marshallable> cmd,
                                        slotid_t i,
                                        slotid_t j,
                                        ballot_t ballot) {
  int n = Config::GetConfig()->GetPartitionSize(par_id);
  auto e = Reactor::CreateSpEvent<CopilotPlusFrontRecoverQuorumEvent>(n, fastQuorumSize(n));

  auto proxies = rpc_par_proxies_[par_id];
  for (auto& p : proxies) {
    auto proxy = (CopilotPlusProxy*) p.second;
    auto site = p.first;

    FutureAttr fuattr;
    fuattr.callback = [e](Future* fu) {
      bool_t y;
      fu->get_reply() >> y;
      e->FeedResponse(y);
    };
    MarshallDeputy md(cmd);
    Future *f = proxy->async_FrontRecover(md, i, j, ballot, fuattr);
    Future::safe_release(f);
  }
  return e;
}

shared_ptr<CopilotPlusFrontCommitQuorumEvent>
CopilotPlusCommo::BroadcastFrontCommit(parid_t par_id,
                                        shared_ptr<Marshallable> cmd,
                                        slotid_t i,
                                        slotid_t j,
                                        ballot_t ballot) {
  int n = Config::GetConfig()->GetPartitionSize(par_id);
  auto e = Reactor::CreateSpEvent<CopilotPlusFrontCommitQuorumEvent>(n, fastQuorumSize(n));

  auto proxies = rpc_par_proxies_[par_id];
  for (auto& p : proxies) {
    auto proxy = (CopilotPlusProxy*) p.second;
    auto site = p.first;

    FutureAttr fuattr;
    fuattr.callback = [e](Future* fu) {
      bool_t y;
      fu->get_reply() >> y;
      e->FeedResponse(y);
    };
    MarshallDeputy md(cmd);
    Future *f = proxy->async_FrontCommit(md, i, j, ballot, fuattr);
    Future::safe_release(f);
  }
  return e;
}

inline int CopilotPlusCommo::maxFailure(int total) {
  return (total + 1) / 2 - 1;
}

inline int CopilotPlusCommo::fastQuorumSize(int total) {
  // TODO: calculate carefully
  return total / 4 * 3 + 1;
}

inline int CopilotPlusCommo::quorumSize(int total) {
  return total - maxFailure(total);
}

inline int CopilotPlusCommo::smallQuorumSize(int total) {
  // TODO: calculate carefully
  return total / 4 + 1;
}

// void CopilotPlusCommo::setServer(CopilotPlusServer *svr) {
//   svr_ = svr;
// }

// CopilotPlusServer* CopilotPlusCommo::getServer() {
//   return svr_;
// }

}