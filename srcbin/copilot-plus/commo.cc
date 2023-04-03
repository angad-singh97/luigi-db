#include "../__dep__.h"
#include "commo.h"

namespace janus {

CopilotPlusCommo::CopilotPlusCommo(PollMgr *poll): Communicator(poll) {

}

void
CopilotPlusCommo::ForwardReply(const parid_t par_id,
                               const slotid_t slot_id,
                               const siteid_t site_id,
                               const slotid_t i,
                               const slotid_t j,
                               const ballot_t ballot,
                               const bool_t accepted) {
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
CopilotPlusCommo::BroadcastFrontRecover(const parid_t par_id,
                                        const slotid_t slot_id,
                                        const shared_ptr<Marshallable> cmd,
                                        const bool_t commit_no_op_,
                                        const slotid_t i,
                                        const slotid_t j,
                                        const ballot_t ballot) {
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
    Future *f = proxy->async_FrontRecover(slot_id, md, commit_no_op_, i, j, ballot, fuattr);
    Future::safe_release(f);
  }
  return e;
}

shared_ptr<CopilotPlusFrontCommitQuorumEvent>
CopilotPlusCommo::BroadcastFrontCommit(const parid_t par_id,
                                        const slotid_t slot_id,
                                        const shared_ptr<Marshallable> cmd,
                                        const bool_t commit_no_op_,
                                        const slotid_t i,
                                        const slotid_t j,
                                        const ballot_t ballot) {
  int n = Config::GetConfig()->GetPartitionSize(par_id);
  auto e = Reactor::CreateSpEvent<CopilotPlusFrontCommitQuorumEvent>(n, fastQuorumSize(n));

  auto proxies = rpc_par_proxies_[par_id];
  for (auto& p : proxies) {
    auto proxy = (CopilotPlusProxy*) p.second;
    auto site = p.first;

    FutureAttr fuattr;
    fuattr.callback = [e](Future* fu) {
      e->FeedResponse(1);
    };
    MarshallDeputy md(cmd);
    Future *f = proxy->async_FrontCommit(slot_id, md, commit_no_op_, i, j, ballot, fuattr);
    Future::safe_release(f);
  }
  return e;
}

// void CopilotPlusCommo::setServer(CopilotPlusServer *svr) {
//   svr_ = svr;
// }

// CopilotPlusServer* CopilotPlusCommo::getServer() {
//   return svr_;
// }

}