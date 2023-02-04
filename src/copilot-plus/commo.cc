#include "../__dep__.h"
#include "commo.h"

namespace janus {

void CopilotPlusSubmitQuorumEvent::FeedResponse(slotid_t i, slotid_t j, ballot_t ballot) {
  responses_.push_back(ResponsePack{i, j, ballot});
}

bool CopilotPlusSubmitQuorumEvent::FastYes() {
  std::sort(responses_.begin(), responses_.end());
  // TODO
  return true;
}

bool CopilotPlusSubmitQuorumEvent::FastNo() {
  std::sort(responses_.begin(), responses_.end());
  // TODO
  return false;
}

CopilotPlusCommo::CopilotPlusCommo(PollMgr *poll): Communicator(poll) {

}

shared_ptr<CopilotPlusSubmitQuorumEvent>
CopilotPlusCommo::BroadcastSubmit(parid_t par_id,
                                  shared_ptr<Marshallable> cmd) {
  int n = Config::GetConfig()->GetPartitionSize(par_id);
  auto e = Reactor::CreateSpEvent<CopilotPlusSubmitQuorumEvent>(n, fastQuorumSize(n));

  auto proxies = rpc_par_proxies_[par_id];
  for (auto& p : proxies) {
    auto proxy = (CopilotPlusProxy*) p.second;
    auto site = p.first;

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


}