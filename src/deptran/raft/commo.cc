
#include "commo.h"
#include "../rcc/graph.h"
#include "../rcc/graph_marshaler.h"
#include "../command.h"
#include "../procedure.h"
#include "../command_marshaler.h"
#include "../rcc_rpc.h"

namespace janus {

// RaftCommo::RaftCommo(PollMgr* poll) : Communicator(poll) {
// //  verify(poll != nullptr);
// }

// shared_ptr<RaftAppendQuorumEvent>
// RaftCommo::BroadcastAppendEntries(parid_t par_id,
//                                       siteid_t leader_site_id,
//                                       slotid_t slot_id,
//                                       bool isLeader,
//                                       uint64_t currentTerm,
//                                       uint64_t prevLogIndex,
//                                       uint64_t prevLogTerm,
//                                       uint64_t commitIndex,
//                                       shared_ptr<Marshallable> cmd) {
//   int n = Config::GetConfig()->GetPartitionSize(par_id);
//   auto e = Reactor::CreateSpEvent<RaftAppendQuorumEvent>(n, n/2 + 1);
//   auto proxies = rpc_par_proxies_[par_id];

//   unordered_set<std::string> ip_addrs {};
//   std::vector<std::shared_ptr<rrr::Client>> clients;

//   vector<Future*> fus;
//   WAN_WAIT;

//   for (auto& p : proxies) {
//     auto id = p.first;
//     auto proxy = (RaftProxy*) p.second;
//     auto cli_it = rpc_clients_.find(id);
//     std::string ip = "";
//     if (cli_it != rpc_clients_.end()) {
//       ip = cli_it->second->host();
// 			//cli = cli_it->second;
//     }
//     ip_addrs.insert(ip);
// 		//clients.push_back(cli);
//   }
//   //e->clients_ = clients;
  
//   for (auto& p : proxies) {
//     auto follower_id = p.first;
//     auto proxy = (RaftProxy*) p.second;
//     auto cli_it = rpc_clients_.find(follower_id);
//     std::string ip = "";
//     if (cli_it != rpc_clients_.end()) {
//       ip = cli_it->second->host();
//     }
// 	if (p.first == leader_site_id) {
//         // fix the 1c1s1p bug
//         // Log_info("leader_site_id %d", leader_site_id);
//         e->FeedResponse(true, prevLogIndex + 1, ip);
//         continue;
//     }
//     FutureAttr fuattr;
//     struct timespec begin;
//     clock_gettime(CLOCK_MONOTONIC, &begin);

//     fuattr.callback = [this, e, isLeader, currentTerm, follower_id, n, ip, begin] (Future* fu) {
//       if (fu->get_error_code() != 0) {
//         Log_info("Get a error message in reply");
//         return;
//       }
//       uint64_t accept = 0;
//       uint64_t term = 0;
//       uint64_t index = 0;
			
//       fu->get_reply() >> term;
// 			fu->get_reply() >> accept;
//       fu->get_reply() >> index;
			
// 			struct timespec end;
// 			//clock_gettime(CLOCK_MONOTONIC, &begin);
// 			this->outbound--;
// 			//Log_info("reply from server: %s and is_ready: %d", ip.c_str(), e->IsReady());
// 			clock_gettime(CLOCK_MONOTONIC, &end);
// 			//Log_info("time of reply on server %d: %ld", follower_id, (end.tv_sec - begin.tv_sec)*1000000000 + end.tv_nsec - begin.tv_nsec);
			
//       bool y = ((accept == 1) && (isLeader) && (currentTerm == term));
//       e->FeedResponse(y, index, ip);
//     };
//     MarshallDeputy md(cmd);
// 		verify(md.sp_data_ != nullptr);
// 		outbound++;
//     auto f = proxy->async_AppendEntries(slot_id,
//                                         currentTerm,
//                                         prevLogIndex,
//                                         prevLogTerm,
//                                         md, 
//                                         commitIndex,
//                                         fuattr);
//     Future::safe_release(f);
//   }
//   verify(!e->IsReady());
//   return e;
// }

// void RaftCommo::BroadcastDecide(const parid_t par_id,
//                                 const slotid_t slot_id,
//                                 const shared_ptr<Marshallable> cmd) {
//   auto proxies = rpc_par_proxies_[par_id];
//   vector<Future*> fus;
//   for (auto& p : proxies) {
//     auto proxy = (RaftProxy*) p.second;
//     FutureAttr fuattr;
//     fuattr.callback = [](Future* fu) {};
//     MarshallDeputy md(cmd);
//     auto f = proxy->async_Decide(slot_id, md, fuattr);
//     Future::safe_release(f);
//   }
// }

// shared_ptr<RaftRequestVoteQuorumEvent>
// RaftCommo::BroadcastRequestVote(parid_t par_id,
//                                 ballot_t candidate_term,
//                                 locid_t candidate_id,
//                                 slotid_t last_log_index,
//                                 ballot_t last_log_term) {
//   int n = Config::GetConfig()->GetPartitionSize(par_id);
//   auto e = Reactor::CreateSpEvent<RaftRequestVoteQuorumEvent>(n, n/2);
//   auto proxies = rpc_par_proxies_[par_id];
//   WAN_WAIT;
//   for (auto& p : proxies) {
//     if (p.first == this->loc_id_)
//         continue;
//     auto proxy = (RaftProxy*) p.second;
//     FutureAttr fuattr;
//     fuattr.callback = [e](Future* fu) {
//       if (fu->get_error_code() != 0) {
//         Log_info("Get a error message in reply");
//         return;
//       }
//       ballot_t term = 0;
//       bool_t vote = false ;
//       fu->get_reply() >> term;
//       fu->get_reply() >> vote ;
//       e->FeedResponse(vote, term);
//       // TODO add max accepted value.
//     };
//     Future::safe_release(proxy->async_RequestVote(candidate_term, candidate_id, last_log_index, last_log_term, fuattr));
//   }
//   return e;
// }

RaftCommo::RaftCommo(PollMgr* poll) : Communicator(poll) {
}

shared_ptr<IntEvent> 
RaftCommo::SendRequestVote(
  parid_t par_id,
  siteid_t site_id,
  uint64_t leaderTerm,
  uint64_t candidateId,
  uint64_t lastLogIndex,
  uint64_t lastLogTerm,
  uint64_t *receiverTerm,
  bool_t *voteGranted
) {
  auto proxies = rpc_par_proxies_[par_id];
  auto ev = Reactor::CreateSpEvent<IntEvent>();
  for (auto &p : proxies) {
    if (p.first == site_id) {
      RaftProxy *proxy = (RaftProxy*) p.second;
      FutureAttr fuattr;

      fuattr.callback = [receiverTerm, voteGranted, ev](Future* fu) {

        fu->get_reply() >> *receiverTerm;
        fu->get_reply() >> *voteGranted;

        if (ev->status_ != Event::TIMEOUT)
          ev->Set(1);
      };
      proxy->async_RequestVote(leaderTerm, candidateId, lastLogIndex, lastLogTerm, fuattr);
      // Call_Async(proxy, RequestVote, 
      //   leaderTerm, candidateId, lastLogIndex, lastLogTerm, 
      //   fuattr);
    }
  }
  // Log_info("[+] SendRequestVote: %d", ev->status_);
  return ev;
}

shared_ptr<IntEvent> 
RaftCommo::SendAppendEntries(
  parid_t par_id,
  siteid_t site_id,
  uint64_t leaderTerm,
  uint64_t leaderId,
  uint64_t prevLogIndex,
  uint64_t prevLogTerm,
  MarshallDeputy entry,
  uint64_t entryTerm,
  // Marshallable entry,
  uint64_t leaderCommit,
  uint64_t* receiverTerm,
  bool_t* success
) {
  auto proxies = rpc_par_proxies_[par_id];
  auto ev = Reactor::CreateSpEvent<IntEvent>();
  for (auto &p : proxies) {
    if (p.first == site_id) {
      RaftProxy *proxy = (RaftProxy*) p.second;
      FutureAttr fuattr;

      fuattr.callback = [receiverTerm, success, ev](Future* fu) {
        fu->get_reply() >> *receiverTerm;
        fu->get_reply() >> *success;

        if (ev->status_ != Event::TIMEOUT)
          ev->Set(1);
      };
      proxy->async_AppendEntries(leaderTerm, leaderId, prevLogIndex, prevLogTerm, entry, entryTerm, leaderCommit, fuattr);
      // Call_Async(proxy, AppendEntries, 
      //   leaderTerm, leaderId, prevLogIndex, prevLogTerm, entry, entryTerm, leaderCommit,
      //   fuattr);
    }
  }
  // Log_info("[+] SendAppendEntries: %d", ev->status_);
  return ev;
}

shared_ptr<IntEvent> 
RaftCommo::SendEmptyAppendEntries(
  parid_t par_id,
  siteid_t site_id,
  uint64_t leaderTerm,
  uint64_t leaderId,
  uint64_t prevLogIndex,
  uint64_t prevLogTerm,
  uint64_t leaderCommit,
  uint64_t* receiverTerm,
  bool_t* success
) {
  auto proxies = rpc_par_proxies_[par_id];
  auto ev = Reactor::CreateSpEvent<IntEvent>();
  for (auto &p : proxies) {
    if (p.first == site_id) {
      RaftProxy *proxy = (RaftProxy*) p.second;
      FutureAttr fuattr;

      fuattr.callback = [receiverTerm, success, ev](Future* fu) {
        fu->get_reply() >> *receiverTerm;
        fu->get_reply() >> *success;
        
        if (ev->status_ != Event::TIMEOUT)
          ev->Set(1);
      };
      proxy->async_EmptyAppendEntries(leaderTerm, leaderId, prevLogIndex, prevLogTerm, leaderCommit, fuattr);
      // Call_Async(proxy, EmptyAppendEntries, 
      //   leaderTerm, leaderId, prevLogIndex, prevLogTerm, leaderCommit,
      //   fuattr);
    }
  }
  // Log_info("[+] SendEmptyAppendEntries: %d", ev->status_);
  return ev;
}

shared_ptr<IntEvent> 
RaftCommo::SendString(parid_t par_id, siteid_t site_id, const string& msg, string* res) {
  auto proxies = rpc_par_proxies_[par_id];
  auto ev = Reactor::CreateSpEvent<IntEvent>();
  for (auto& p : proxies) {
    if (p.first == site_id) {
      RaftProxy *proxy = (RaftProxy*) p.second;
      FutureAttr fuattr;
      fuattr.callback = [res,ev](Future* fu) {
        fu->get_reply() >> *res;
        ev->Set(1);
      };
      /* wrap Marshallable in a MarshallDeputy to send over RPC */
      proxy->async_HelloRpc(msg, fuattr);
      // Call_Async(proxy, HelloRpc, msg, fuattr);
    }
  }
  return ev;
}


} // namespace janus
