
#include "commo.h"
#include "../rcc/graph.h"
#include "../rcc/graph_marshaler.h"
#include "../command.h"
#include "../procedure.h"
#include "../command_marshaler.h"
#include "../rcc_rpc.h"

namespace janus {

MenciusCommo::MenciusCommo(PollMgr* poll) : Communicator(poll) {
//  verify(poll != nullptr);
}

void MenciusCommo::BroadcastPrepare(parid_t par_id,
                                       slotid_t slot_id,
                                       ballot_t ballot,
                                       const function<void(Future*)>& cb) {
  verify(0); // deprecated function
  auto proxies = rpc_par_proxies_[par_id];
  auto leader_id = LeaderProxyForPartition(par_id).first;
  for (auto& p : proxies) {
    auto proxy = (MenciusProxy*) p.second;
    FutureAttr fuattr;
    fuattr.callback = cb;
    Future::safe_release(proxy->async_Prepare(slot_id, ballot, fuattr));
  }
}

shared_ptr<MenciusPrepareQuorumEvent>
MenciusCommo::BroadcastPrepare(parid_t par_id,
                                  slotid_t slot_id,
                                  ballot_t ballot) {
  verify(0);
  int n = Config::GetConfig()->GetPartitionSize(par_id);
  auto e = Reactor::CreateSpEvent<MenciusPrepareQuorumEvent>(n, n/2+1);
  auto src_coroid = e->GetCoroId();
  auto proxies = rpc_par_proxies_[par_id];

  WAN_WAIT;
  auto leader_id = LeaderProxyForPartition(par_id).first;
  for (auto& p : proxies) {
    auto proxy = (MenciusProxy*) p.second;
    auto follower_id = p.first;
    // e->add_dep(leader_id, src_coroid, follower_id, -1);

    FutureAttr fuattr;
    fuattr.callback = [e, ballot, leader_id, src_coroid, follower_id](Future* fu) {
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

shared_ptr<MenciusSuggestQuorumEvent>
MenciusCommo::BroadcastSuggest(parid_t par_id,
                                 slotid_t slot_id,
                                 ballot_t ballot,
                                 shared_ptr<Marshallable> cmd) {
  Log_info("invoke BroadcastSuggest, slot_id:%d", slot_id);
  int n = Config::GetConfig()->GetPartitionSize(par_id);
  auto e = Reactor::CreateSpEvent<MenciusSuggestQuorumEvent>(n, n/2+1);
//  auto e = Reactor::CreateSpEvent<MenciusSuggestQuorumEvent>(n, n);

  auto src_coroid = e->GetCoroId();
  auto proxies = rpc_par_proxies_[par_id];
  auto leader_id = LeaderProxyForPartition(par_id, (slot_id-1)%n).first;
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
    auto proxy = (MenciusProxy*) p.second;
    auto follower_id = p.first;

    // e->add_dep(leader_id, src_coroid, follower_id, -1);

    FutureAttr fuattr;
    fuattr.callback = [e, start, ballot, leader_id, src_coroid, follower_id] (Future* fu) {
      ballot_t b = 0;
      uint64_t coro_id = 0;
      fu->get_reply() >> b >> coro_id;
      e->FeedResponse(b==ballot);
      auto end = chrono::system_clock::now();
      auto duration = chrono::duration_cast<chrono::microseconds>(end-start).count();
      //Log_info("The duration of Suggest() for %d is: %d", follower_id, duration);
      // e->deps[leader_id][src_coroid][follower_id].erase(-1);
      // e->deps[leader_id][src_coroid][follower_id].insert(coro_id);
    };
    MarshallDeputy md(cmd);
    auto start1 = chrono::system_clock::now();
    uint64_t sender = loc_id_;
    std::vector<uint64_t> skip_commits;
    std::vector<uint64_t> skip_potentials;
    // TODO:
    //  1. from skip_potentials_recd (in server.h) to compute the committed SKIP entries (as well alpha)  => skip_commits
    //  2. from logs_ to compute potential SKIP entries => skip_potentials
    
    auto f = proxy->async_Suggest(slot_id, start_, ballot, sender, skip_commits, skip_potentials, md, fuattr);
    auto end1 = chrono::system_clock::now();
    auto duration = chrono::duration_cast<chrono::microseconds>(end1-start1).count();
    Future::safe_release(f);
  }
  return e;
}

void MenciusCommo::BroadcastSuggest(parid_t par_id,
                                      slotid_t slot_id,
                                      ballot_t ballot,
                                      shared_ptr<Marshallable> cmd,
                                      const function<void(Future*)>& cb) {
  verify(0); // deprecated function
  // auto proxies = rpc_par_proxies_[par_id];
  // auto leader_id = LeaderProxyForPartition(par_id).first;
  // vector<Future*> fus;
  // for (auto& p : proxies) {
  //   auto proxy = (MenciusProxy*) p.second;
  //   FutureAttr fuattr;
  //   fuattr.callback = cb;
  //   MarshallDeputy md(cmd);
  //   uint64_t time = 0; // compiles the code
  //   std::vector<uint64_t> skip_commits(1);
  //   skip_commits.push_back(100);
  //   std::vector<uint64_t> skip_potentials(1);
  //   skip_potentials.push_back(200);
  //   auto f = proxy->async_Suggest(slot_id, time,ballot, skip_commits, skip_potentials, md, fuattr);
  //   Future::safe_release(f);
  // }
}

void MenciusCommo::BroadcastDecide(const parid_t par_id,
                                      const slotid_t slot_id,
                                      const ballot_t ballot,
                                      const shared_ptr<Marshallable> cmd) {
  Log_info("invoke BroadcastDecide, slot_id:%d", slot_id);
  auto proxies = rpc_par_proxies_[par_id];
  int n = proxies.size();
  auto leader_id = LeaderProxyForPartition(par_id, (slot_id-1)%n).first;
  vector<Future*> fus;
  for (auto& p : proxies) {
    auto proxy = (MenciusProxy*) p.second;
    FutureAttr fuattr;
    fuattr.callback = [](Future* fu) {};
    MarshallDeputy md(cmd);
    auto f = proxy->async_Decide(slot_id, ballot, md, fuattr);
    //sp_quorum_event->add_dep(leader_id, p.first);
    Future::safe_release(f);
  }
}

} // namespace janus
