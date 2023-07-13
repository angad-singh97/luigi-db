
#include "commo.h"
#include "../rcc/graph.h"
#include "../rcc/graph_marshaler.h"
#include "../command.h"
#include "../procedure.h"
#include "../command_marshaler.h"
#include "../rcc_rpc.h"

namespace janus {

void CurpPlusPrepareQuorumEvent::FeedResponse(bool y,
                                                ballot_t seen_ballot,
                                                int last_accepted_status,
                                                MarshallDeputy last_accepted_md_cmd,
                                                ballot_t last_accepted_ballot) {
  count_++;
  max_seen_ballot_ = max(max_seen_ballot_, seen_ballot);
  if (y) {
    shared_ptr<CmdData> last_accepted_cmd = dynamic_pointer_cast<CmdData>(const_cast<MarshallDeputy&>(last_accepted_md_cmd).sp_data_);
    pair<int, int> cmd_id = {last_accepted_cmd->client_id_, last_accepted_cmd->cmd_id_in_client_};
    accepted_cmds_.push_back(AcceptedCmd{cmd_id, last_accepted_status, last_accepted_cmd, last_accepted_ballot});
    VoteYes();
  } else {
    VoteNo();
  }
}

bool CurpPlusPrepareQuorumEvent::CommitYes() {
  ready_cmd_ = nullptr;
  if (count_ <= quorum_) return false;
  for (int i = 0; i < accepted_cmds_.size(); i++) {
    if (accepted_cmds_[i].last_accepted_status == CurpPlusData::CurpPlusStatus::committed) {
      ready_cmd_ = accepted_cmds_[i].last_accepted_cmd;
      return true;
    }
  }
  return false;
}

bool CurpPlusPrepareQuorumEvent::AcceptYes() {
  ready_cmd_ = nullptr;
  ballot_t max_ballot_of_ready_cmd_ = -1;
  if (count_ <= quorum_) return false;
  for (int i = 0; i < accepted_cmds_.size(); i++) {
    if (accepted_cmds_[i].last_accepted_status == CurpPlusData::CurpPlusStatus::accepted
        && accepted_cmds_[i].last_accepted_ballot > max_ballot_of_ready_cmd_) {
      ready_cmd_ = accepted_cmds_[i].last_accepted_cmd;
      max_ballot_of_ready_cmd_ = accepted_cmds_[i].last_accepted_ballot;
    }
  }
  return ready_cmd_ != nullptr;
}

bool CurpPlusPrepareQuorumEvent::FastAcceptYes() {
  // TODO
  return true;
}

bool CurpPlusPrepareQuorumEvent::AcceptAnyYes() {
  ready_cmd_ = nullptr;
  if (count_ <= quorum_) return false;
  // TODO: check whether we can pick anyone if no commit / accept
  ready_cmd_ = accepted_cmds_[0].last_accepted_cmd;
  return true;
}

void CurpPlusAcceptQuorumEvent::FeedResponse(bool y, ballot_t seen_ballot) {
  max_seen_ballot_ = max(max_seen_ballot_, seen_ballot);
}

CurpPlusCommo::CurpPlusCommo(PollMgr* poll) : Communicator(poll) {
}

shared_ptr<IntEvent>
CurpPlusCommo::ForwardResultToCoordinator(parid_t par_id,
                                            const shared_ptr<Marshallable>& cmd,
                                            Position pos,
                                            bool_t accepted) {
  int n = Config::GetConfig()->GetPartitionSize(par_id);
  auto e = Reactor::CreateSpEvent<IntEvent>();
  auto proxies = rpc_par_proxies_[par_id];
  MarshallDeputy pos_deputy(make_shared<Position>(pos)), cmd_deputy(cmd);
  for (auto& p : proxies) {
    auto proxy = (CurpPlusProxy *)p.second;
    auto site = p.first;
    // TODO: generelize
    if (0 == site) {
      FutureAttr fuattr;
      fuattr.callback = [](Future *fu) {};

#ifdef CURP_TIME_DEBUG
      struct timeval tp;
      gettimeofday(&tp, NULL);
      Log_info("[CURP] [2-] [tx=%d] Forward %.3f", dynamic_pointer_cast<TpcCommitCommand>(cmd)->tx_id_, tp.tv_sec * 1000 + tp.tv_usec / 1000.0);
#endif

      Future *f = proxy->async_Forward(pos_deputy, cmd_deputy, accepted, fuattr);
      Future::safe_release(f);
    }
  }
  return e;
}

shared_ptr<CurpPlusCoordinatorAcceptQuorumEvent>
CurpPlusCommo::BroadcastCoordinatorAccept(parid_t par_id,
                          shared_ptr<Position> pos,
                          shared_ptr<Marshallable> cmd) {
  int n = Config::GetConfig()->GetPartitionSize(par_id);
  auto e = Reactor::CreateSpEvent<CurpPlusCoordinatorAcceptQuorumEvent>(n);
  auto proxies = rpc_par_proxies_[par_id];
  MarshallDeputy pos_deputy(dynamic_pointer_cast<Marshallable>(pos)), cmd_deputy(cmd);
  for (auto& p : proxies) {
    auto proxy = (CurpPlusProxy *)p.second;
    auto site = p.first;
    FutureAttr fuattr;
    fuattr.callback = [e](Future *fu) {
      bool_t accepted;
      fu->get_reply() >> accepted;
      e->FeedResponse(accepted);
    };
    Future *f = proxy->async_CoordinatorAccept(pos_deputy, cmd_deputy, fuattr);
    Future::safe_release(f);
  }
  return e;
}

shared_ptr<CurpPlusPrepareQuorumEvent>
CurpPlusCommo::BroadcastPrepare(parid_t par_id,
                  shared_ptr<Position> pos,
                  ballot_t ballot) {
  int n = Config::GetConfig()->GetPartitionSize(par_id);
  auto e = Reactor::CreateSpEvent<CurpPlusPrepareQuorumEvent>(n);
  auto proxies = rpc_par_proxies_[par_id];
  MarshallDeputy pos_deputy(dynamic_pointer_cast<Marshallable>(pos));
  for (auto& p : proxies) {
    auto proxy = (CurpPlusProxy *)p.second;
    auto site = p.first;
    FutureAttr fuattr;
    fuattr.callback = [e](Future *fu) {
      bool_t accepted;
      ballot_t seen_ballot;
      i32 last_accepted_status;
      MarshallDeputy last_accepted_cmd;
      ballot_t last_accepted_ballot;
      fu->get_reply() >> accepted >> seen_ballot >> last_accepted_status >> last_accepted_cmd >> last_accepted_ballot;
      e->FeedResponse(accepted, seen_ballot, last_accepted_status, last_accepted_cmd, last_accepted_ballot);
    };
    Future *f = proxy->async_Prepare(pos_deputy, ballot, fuattr);
    Future::safe_release(f);
  }
  return e;
}

shared_ptr<CurpPlusAcceptQuorumEvent>
CurpPlusCommo::BroadcastAccept(parid_t par_id,
                shared_ptr<Position> pos,
                shared_ptr<Marshallable> cmd,
                ballot_t ballot) {
  int n = Config::GetConfig()->GetPartitionSize(par_id);
  auto e = Reactor::CreateSpEvent<CurpPlusAcceptQuorumEvent>(n);
  auto proxies = rpc_par_proxies_[par_id];
  MarshallDeputy pos_deputy(dynamic_pointer_cast<Marshallable>(pos)), cmd_deputy(cmd);
  for (auto& p : proxies) {
    auto proxy = (CurpPlusProxy *)p.second;
    auto site = p.first;
    FutureAttr fuattr;
    fuattr.callback = [e](Future *fu) {
      bool_t accepted;
      ballot_t seen_ballot;
      fu->get_reply() >> accepted >> seen_ballot;
      e->FeedResponse(accepted, seen_ballot);
    };
    Future *f = proxy->async_Accept(pos_deputy, cmd_deputy, ballot, fuattr);
    Future::safe_release(f);
  }
  return e;
}

shared_ptr<IntEvent>
CurpPlusCommo::BroadcastCommit(parid_t par_id,
                                    shared_ptr<Position> pos,
                                    shared_ptr<Marshallable> cmd) {
  int n = Config::GetConfig()->GetPartitionSize(par_id);
  auto e = Reactor::CreateSpEvent<IntEvent>();
  auto proxies = rpc_par_proxies_[par_id];
  MarshallDeputy pos_deputy(dynamic_pointer_cast<Marshallable>(pos)), cmd_deputy(cmd);
  for (auto& p : proxies) {
    auto proxy = (CurpPlusProxy *)p.second;
    auto site = p.first;
    FutureAttr fuattr;
    fuattr.callback = [](Future *fu) {};
    Future *f = proxy->async_Commit(pos_deputy, cmd_deputy, fuattr);
    Future::safe_release(f);
  }
  return e;
}

inline int maxFailure(int total) {
  return (total + 1) / 2 - 1;
}

inline int fastQuorumSize(int total) {
  int max_fail = maxFailure(total);
  return max_fail + (max_fail + 1) / 2 + 1;
}

inline int quorumSize(int total) {
  return total - maxFailure(total);
}

} // namespace janus
