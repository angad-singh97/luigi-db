#include "../__dep__.h"
#include "commo.h"

namespace janus {

void CopilotPlusSubmitQuorumEvent::FeedResponse(slotid_t i, slotid_t j, ballot_t ballot) {
  responses_.push_back(ResponsePack{i, j, ballot});
}

bool CopilotPlusSubmitQuorumEvent::FastYes() {
  int max_len = FindMax();
  return max_len >= CopilotPlusCommo::fastQuorumSize(n_total_);
}

bool CopilotPlusSubmitQuorumEvent::RecoverWithOpYes() {
  int max_len = FindMax();
  return (n_total_ >= quorum_) && (max_len >= CopilotPlusCommo::smallQuorumSize(n_total_));
}

bool CopilotPlusSubmitQuorumEvent::RecoverWithoutOpYes() {
  int max_len = FindMax();
  return (n_total_ >= quorum_) && (max_len < CopilotPlusCommo::smallQuorumSize(n_total_));
}

CopilotPlusCommo::CopilotPlusCommo(PollMgr *poll): Communicator(poll) {

}

shared_ptr<CopilotPlusSubmitQuorumEvent>
CopilotPlusCommo::BroadcastSubmit(parid_t par_id,
                                  shared_ptr<Marshallable> cmd) {
  Log_info("enter BroadcastSubmit");
  int n = Config::GetConfig()->GetPartitionSize(par_id);
  auto e = Reactor::CreateSpEvent<CopilotPlusSubmitQuorumEvent>(n, quorumSize(n));
  Log_info("after CreateSpEvent");
  auto proxies = rpc_par_proxies_[par_id];
  for (auto& p : proxies) {
    auto proxy = (CopilotPlusProxy*) p.second;
    auto site = p.first;
    Log_info("site id = %d", site);
    FutureAttr fuattr;
    fuattr.callback = [e](Future* fu) {
      slotid_t i, j;
      ballot_t ballot;
      fu->get_reply() >> i >> j >> ballot;
      e->FeedResponse(i, j, ballot);
    };
    MarshallDeputy md(cmd);
    Future *f = proxy->async_Submit(md, fuattr);
    Future::safe_release(f);
  }
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