
#include "commo.h"
#include "../rcc/graph.h"
#include "../rcc/graph_marshaler.h"
#include "../command.h"
#include "../procedure.h"
#include "../command_marshaler.h"
#include "../rcc_rpc.h"

namespace janus {

MultiPaxosPlusCommo::MultiPaxosPlusCommo(PollMgr* poll) : Communicator(poll) {
//  verify(poll != nullptr);
}

shared_ptr<PaxosPlusPrepareQuorumEvent>
MultiPaxosPlusCommo::SendForward(parid_t par_id,
                             uint64_t follower_id,
                             uint64_t dep_id,
                             shared_ptr<Marshallable> cmd){
  auto e = Reactor::CreateSpEvent<PaxosPlusPrepareQuorumEvent>(1, 1);
  auto src_coroid = e->GetCoroId();
  auto leader_id = LeaderProxyForPartition(par_id).first;
  auto leader_proxy = (MultiPaxosPlusProxy*) LeaderProxyForPartition(par_id).second;

  FutureAttr fuattr;
  fuattr.callback = [e, leader_id, src_coroid, follower_id](Future* fu) {
    if (fu->get_error_code() != 0) {
      Log_info("Get a error message in reply");
      return;
    }
    uint64_t coro_id = 0;
    fu->get_reply() >> coro_id;
    e->FeedResponse(1);
    Log_info("adding dependency");
    // e->add_dep(follower_id, src_coroid, leader_id, coro_id);
  };

  MarshallDeputy md(cmd);
  Future::safe_release(leader_proxy->async_Forward(md, dep_id));

  return e;
}

void MultiPaxosPlusCommo::BroadcastPrepare(parid_t par_id,
                                       slotid_t slot_id,
                                       ballot_t ballot,
                                       const function<void(Future*)>& cb) {
  verify(0); // deprecated function
  auto proxies = rpc_par_proxies_[par_id];
  auto leader_id = LeaderProxyForPartition(par_id).first;
  for (auto& p : proxies) {
    auto proxy = (MultiPaxosPlusProxy*) p.second;
    FutureAttr fuattr;
    fuattr.callback = cb;
    Future::safe_release(proxy->async_Prepare(slot_id, ballot, fuattr));
  }
}

shared_ptr<PaxosPlusPrepareQuorumEvent>
MultiPaxosPlusCommo::BroadcastPrepare(parid_t par_id,
                                  slotid_t slot_id,
                                  ballot_t ballot) {
  int n = Config::GetConfig()->GetPartitionSize(par_id);
  auto e = Reactor::CreateSpEvent<PaxosPlusPrepareQuorumEvent>(n, n/2+1);
  auto src_coroid = e->GetCoroId();
  auto proxies = rpc_par_proxies_[par_id];

  WAN_WAIT;
  auto leader_id = LeaderProxyForPartition(par_id).first;
  for (auto& p : proxies) {
    auto proxy = (MultiPaxosPlusProxy*) p.second;
    auto follower_id = p.first;
    // e->add_dep(leader_id, src_coroid, follower_id, -1);

    FutureAttr fuattr;
    fuattr.callback = [e, ballot, leader_id, src_coroid, follower_id](Future* fu) {
      if (fu->get_error_code() != 0) {
          Log_info("Get a error message in reply");
          return;
        }
      ballot_t b = 0;
      uint64_t coro_id = 0;
      fu->get_reply() >> b >> coro_id;
      e->FeedResponse(b==ballot);
      // e->deps[leader_id][src_coroid][follower_id].erase(-1);
      // e->deps[leader_id][src_coroid][follower_id].insert(coro_id);
      // TODO add max accepted value.
    };
    Future::safe_release(proxy->async_Prepare(slot_id, ballot, fuattr));
  }
  return e;
}

shared_ptr<PaxosPlusAcceptQuorumEvent>
MultiPaxosPlusCommo::BroadcastAccept(parid_t par_id,
                                 slotid_t slot_id,
                                 ballot_t ballot,
                                 shared_ptr<Marshallable> cmd,
                                 uint64_t curp_need_finish,
                                 TxLogServer *sch) {
  int n = Config::GetConfig()->GetPartitionSize(par_id);
  auto e = Reactor::CreateSpEvent<PaxosPlusAcceptQuorumEvent>(n, n/2+1);
//  auto e = Reactor::CreateSpEvent<PaxosPlusAcceptQuorumEvent>(n, n);

  auto src_coroid = e->GetCoroId();
  auto proxies = rpc_par_proxies_[par_id];
  auto leader_id = LeaderProxyForPartition(par_id).first; // might need to be changed to coordinator's id
  vector<Future*> fus;
  auto start = chrono::system_clock::now();

  time_t tstart = chrono::system_clock::to_time_t(start);
  tm * date = localtime(&tstart);
  date->tm_hour = 0;
  date->tm_min = 0;
  date->tm_sec = 0;
  auto midn = std::chrono::system_clock::from_time_t(std::mktime(date));

  auto hours = chrono::duration_cast<chrono::hours>(start-midn);
  auto minutes = chrono::duration_cast<chrono::minutes>(start-midn);

  auto start_ = chrono::duration_cast<chrono::microseconds>(start-midn-hours-minutes).count();
  WAN_WAIT;
  for (auto& p : proxies) {
    auto proxy = (MultiPaxosPlusProxy*) p.second;
    auto follower_id = p.first;

    // e->add_dep(leader_id, src_coroid, follower_id, -1);

    FutureAttr fuattr;
    fuattr.callback = [e, start, ballot, leader_id, src_coroid, follower_id, cmd, sch] (Future* fu) {
      if (fu->get_error_code() != 0) {
        Log_info("Get a error message in reply");
        return;
      }
      ballot_t b = 0;
      uint64_t coro_id = 0;
      fu->get_reply() >> b >> coro_id;

      // curp part
      bool_t finish_accept = 0;
      uint64_t finish_ver = 0;
      fu->get_reply() >> finish_accept >> finish_ver;
      pair<int32_t, int32_t> cmd_id = SimpleRWCommand::GetCmdID(cmd);
      sch->CurpAttemptCommitFinishReply(cmd_id, finish_accept, finish_ver);

#ifdef CURP_AVOID_CurpSkipFastpath_DEBUG
      Log_info("About to FeedResponse for cmd<%d, %d> at %.2f", cmd_id.first, cmd_id.second, SimpleRWCommand::GetCurrentMsTime());
#endif
      e->FeedResponse(b==ballot);

      auto end = chrono::system_clock::now();
      auto duration = chrono::duration_cast<chrono::microseconds>(end-start).count();
      //Log_info("The duration of Accept() for %d is: %d", follower_id, duration);
      // e->deps[leader_id][src_coroid][follower_id].erase(-1);
      // e->deps[leader_id][src_coroid][follower_id].insert(coro_id);
    };
    MarshallDeputy md(cmd);
    auto start1 = chrono::system_clock::now();
    auto f = proxy->async_Accept(slot_id, start_, ballot, md, curp_need_finish, fuattr);
    auto end1 = chrono::system_clock::now();
    auto duration = chrono::duration_cast<chrono::microseconds>(end1-start1).count();
    //Log_info("Time for Async_Accept() for %d is: %d", follower_id, duration);
    Future::safe_release(f);
  }
  return e;
}

// void MultiPaxosPlusCommo::BroadcastAccept(parid_t par_id,
//                                       slotid_t slot_id,
//                                       ballot_t ballot,
//                                       shared_ptr<Marshallable> cmd,
//                                       const function<void(Future*)>& cb) {
//   verify(0); // deprecated function
//   auto proxies = rpc_par_proxies_[par_id];
//   auto leader_id = LeaderProxyForPartition(par_id).first;
//   vector<Future*> fus;
//   for (auto& p : proxies) {
//     auto proxy = (MultiPaxosPlusProxy*) p.second;
//     FutureAttr fuattr;
//     fuattr.callback = cb;
//     MarshallDeputy md(cmd);
//     uint64_t time = 0; // compiles the code
//     auto f = proxy->async_Accept(slot_id, time,ballot, md, fuattr);
//     Future::safe_release(f);
//   }
// //  verify(0);
// }

void MultiPaxosPlusCommo::BroadcastDecide(const parid_t par_id,
                                      const slotid_t slot_id,
                                      const ballot_t ballot,
                                      const shared_ptr<Marshallable> cmd) {
  auto proxies = rpc_par_proxies_[par_id];
  auto leader_id = LeaderProxyForPartition(par_id).first;
  vector<Future*> fus;
  for (auto& p : proxies) {
    auto proxy = (MultiPaxosPlusProxy*) p.second;
    FutureAttr fuattr;
    fuattr.callback = [](Future* fu) {};
    MarshallDeputy md(cmd);
    auto f = proxy->async_Decide(slot_id, ballot, md, fuattr);
    //sp_quorum_event->add_dep(leader_id, p.first);
    Future::safe_release(f);
  }
}

shared_ptr<IntEvent> MultiPaxosPlusCommo::BroadcastTest() {
  auto proxies = rpc_par_proxies_[0];
  auto e = Reactor::CreateSpEvent<IntEvent>();
  for (auto& p : proxies) {
    auto proxy = (CurpProxy*) p.second;
    FutureAttr fuattr;
    fuattr.callback = [e](Future* fu) {
      if (fu->get_error_code() != 0) {
        Log_info("Get a error message in reply");
        return;
      }
      int32_t b;
      fu->get_reply() >> b;
      verify(b == 24);
      Log_info("[CURP] received replied 24");
      e->Set(1);
    };
    auto f = proxy->async_CurpTest(42, fuattr);
    Future::safe_release(f);
  }
  return e;
}

} // namespace janus
