
#include "commo.h"
#include "../rcc/graph.h"
#include "../rcc/graph_marshaler.h"
#include "../command.h"
#include "../procedure.h"
#include "../command_marshaler.h"
#include "../rcc_rpc.h"

namespace janus {



CurpPlusCommo::CurpPlusCommo(PollMgr* poll) : Communicator(poll) {
}


// shared_ptr<CurpDispatchQuorumEvent>
// Communicator::CurpBroadcastDispatch(
//     shared_ptr<vector<shared_ptr<TxPieceData>>> sp_vec_piece) {
//   // cmdid_t cmd_id = sp_vec_piece->at(0)->root_id_;
//   verify(!sp_vec_piece->empty());
//   auto par_id = sp_vec_piece->at(0)->PartitionId();
  
//   shared_ptr<VecPieceData> sp_vpd(new VecPieceData);
//   sp_vpd->sp_vec_piece_data_ = sp_vec_piece;
//   MarshallDeputy md(sp_vpd);

//   int n = Config::GetConfig()->GetPartitionSize(par_id);
//   auto e = Reactor::CreateSpEvent<CurpDispatchQuorumEvent>(n, quorumSize(n));

//   for (auto& pair : rpc_par_proxies_[par_id]) {
//     rrr::FutureAttr fuattr;
//     fuattr.callback =
//         [e, this](Future* fu) {
//           // Log_info("[copilot+] callback function [1] in Communicator::CurpBroadcastDispatch");
//           bool_t accepted;
//           MarshallDeputy pos_deputy;
//           value_t result;
//           siteid_t coo_id;
//           fu->get_reply() >> accepted >> pos_deputy >> result >> coo_id;
//           e->FeedResponse(accepted, *dynamic_pointer_cast<Position>(pos_deputy.sp_data_).get(), result, coo_id);
//         };
    
//     DepId di;
//     di.str = "dep";
//     di.id = Communicator::global_id++;
    
//     auto proxy = (CurpPlusProxy *)pair.second;
//     auto future = proxy->async_Dispatch(sp_vec_piece->at(0)->client_id_, sp_vec_piece->at(0)->cmd_id_in_client_, md, fuattr);
//     Future::safe_release(future);
//   }

//   e->Wait();

//   return e;
// }

// shared_ptr<CurpDispatchQuorumEvent>
// CurpPlusCommo::CurpBroadcastDispatch(
//     shared_ptr<vector<shared_ptr<TxPieceData>>> sp_vec_piece) {
//   verify(0);
//   // cmdid_t cmd_id = sp_vec_piece->at(0)->root_id_;
//   verify(!sp_vec_piece->empty());
//   auto par_id = sp_vec_piece->at(0)->PartitionId();
  
//   shared_ptr<VecPieceData> sp_vpd(new VecPieceData);
//   sp_vpd->sp_vec_piece_data_ = sp_vec_piece;
//   MarshallDeputy md(sp_vpd);

//   int n = Config::GetConfig()->GetPartitionSize(par_id);
//   auto e = Reactor::CreateSpEvent<CurpDispatchQuorumEvent>(n, CurpQuorumSize(n));

//   for (auto& pair : rpc_par_proxies_[par_id]) {
//     rrr::FutureAttr fuattr;
//     fuattr.callback =
//         [e, this](Future* fu) {
//           // Log_info("[copilot+] callback function [1] in Communicator::CurpBroadcastDispatch");
//           bool_t accepted;
//           pos_t pos0, pos1;
//           value_t result;
//           siteid_t coo_id;
//           fu->get_reply() >> accepted >> pos0 >> pos1 >> result >> coo_id;
//           e->FeedResponse(accepted, pos0, pos1, result, coo_id);
//         };
    
//     DepId di;
//     di.str = "dep";
//     di.id = Communicator::global_id++;
    
//     auto proxy = (CurpPlusProxy *)pair.second;
//     auto future = proxy->async_PoorDispatch(sp_vec_piece->at(0)->client_id_, sp_vec_piece->at(0)->cmd_id_in_client_, md, fuattr);
//     Future::safe_release(future);
//   }

//   e->Wait();

//   return e;
// }

// shared_ptr<CurpDispatchQuorumEvent>
// CurpPlusCommo::CurpBroadcastDispatch(shared_ptr<Marshallable> cmd) {
//   shared_ptr<TpcCommitCommand> tpc_cmd = dynamic_pointer_cast<TpcCommitCommand>(cmd);
//   VecPieceData *cmd_cast = (VecPieceData*)(tpc_cmd->cmd_.get());
//   shared_ptr<vector<shared_ptr<TxPieceData>>> sp_vec_piece = cmd_cast->sp_vec_piece_data_;
//   verify(!sp_vec_piece->empty());
//   auto par_id = sp_vec_piece->at(0)->PartitionId();
  
//   shared_ptr<VecPieceData> sp_vpd(new VecPieceData);
//   sp_vpd->sp_vec_piece_data_ = sp_vec_piece;
//   MarshallDeputy md(cmd);

//   int n = Config::GetConfig()->GetPartitionSize(par_id);
//   auto e = Reactor::CreateSpEvent<CurpDispatchQuorumEvent>(n, CurpQuorumSize(n));

//   for (auto& pair : rpc_par_proxies_[par_id]) {
//     rrr::FutureAttr fuattr;
//     fuattr.callback =
//         [e, this](Future* fu) {
//           // Log_info("[copilot+] callback function [1] in Communicator::CurpBroadcastDispatch");
//           bool_t accepted;
//           pos_t pos0, pos1;
//           value_t result;
//           siteid_t coo_id;
//           fu->get_reply() >> accepted;
//           fu->get_reply() >> pos0 ;
//           fu->get_reply() >> pos1 ;
//           fu->get_reply() >> result ;
//           fu->get_reply() >> coo_id;
//           // fu->get_reply() >> accepted >> pos0 >> pos1 >> result >> coo_id;
//           e->FeedResponse(accepted, pos0, pos1, result, coo_id);
//         };
    
//     rrr::FutureAttr test_fuattr;
//     test_fuattr.callback =
//         [e, this](Future* fu) {
//           int32_t b;
//           fu->get_reply() >> b;
//           verify(b == 24);
//           Log_info("[CURP] Client Received Reply 24");
//         };
    
//     DepId di;
//     di.str = "dep";
//     di.id = Communicator::global_id++;
    
//     auto proxy = (CurpPlusProxy *)pair.second;

// #ifdef CURP_TIME_DEBUG
//     struct timeval tp;
//     gettimeofday(&tp, NULL);
//     Log_info("[CURP] [1-] [tx=%d] async_PoorDispatch called by Submit %.3f", tpc_cmd->tx_id_, tp.tv_sec * 1000 + tp.tv_usec / 1000.0);
// #endif

//     Log_info("[CURP] Before async_Test");

//     int32_t b;
//     proxy->Test(42, &b);
//     // Future::safe_release(proxy->Test(42, &b));

//     // auto future = proxy->async_PoorDispatch(sp_vec_piece->at(0)->client_id_, sp_vec_piece->at(0)->cmd_id_in_client_, md, fuattr);
//     // Future::safe_release(future);
//   }

//   e->Wait();

//   return e;
// }

// shared_ptr<QuorumEvent> 
// CurpPlusCommo::DirectCurpBroadcastWaitCommit(shared_ptr<vector<shared_ptr<SimpleCommand>>> vec_piece_data,
//                                             siteid_t coo_id) {
//   // cmdid_t cmd_id = sp_vec_piece->at(0)->root_id_;
//   int32_t client_id_ = vec_piece_data->at(0)->client_id_;
//   int32_t cmd_id_in_client_ = vec_piece_data->at(0)->cmd_id_in_client_;
//   auto par_id = vec_piece_data->at(0)->PartitionId();
//   auto e = Reactor::CreateSpEvent<QuorumEvent>(1, 1);

//   shared_ptr<VecPieceData> sp_vpd(new VecPieceData);
//   // sp_vpd->sp_vec_piece_data_ = sp_vec_piece;
//   sp_vpd->sp_vec_piece_data_ = vec_piece_data;
//   MarshallDeputy md(sp_vpd);

//   for (auto& pair : rpc_par_proxies_[par_id])
//     if (pair.first == coo_id) {
//       rrr::FutureAttr fuattr;
//       fuattr.callback =
//           [e](Future* fu) {
//             bool_t committed;
//             fu->get_reply() >> committed;
//             if (committed)
//               e->VoteYes();
//             else
//               e->VoteNo();
//           };
      
//       auto proxy = (CurpPlusProxy *)pair.second;
//       auto future = proxy->async_WaitCommit(client_id_, cmd_id_in_client_, fuattr);
//       Future::safe_release(future);
//   }
//   return e;
// }


// shared_ptr<QuorumEvent>
// CurpPlusCommo::DirectCurpBroadcastWaitCommit(shared_ptr<Marshallable> cmd,
//                                             siteid_t coo_id) {
//   shared_ptr<TpcCommitCommand> tpc_cmd = dynamic_pointer_cast<TpcCommitCommand>(cmd);
//   VecPieceData *cmd_cast = (VecPieceData*)(tpc_cmd->cmd_.get());
//   shared_ptr<vector<shared_ptr<TxPieceData>>> sp_vec_piece = cmd_cast->sp_vec_piece_data_;
//   return DirectCurpBroadcastWaitCommit(sp_vec_piece, coo_id);
// }

// shared_ptr<IntEvent>
// CurpPlusCommo::ForwardResultToCoordinator(parid_t par_id,
//                                             const shared_ptr<Marshallable>& cmd,
//                                             Position pos,
//                                             bool_t accepted) {
//   int n = Config::GetConfig()->GetPartitionSize(par_id);
//   auto e = Reactor::CreateSpEvent<IntEvent>();
//   auto proxies = rpc_par_proxies_[par_id];
//   MarshallDeputy pos_deputy(make_shared<Position>(pos)), cmd_deputy(cmd);
//   for (auto& p : proxies) {
//     auto proxy = (CurpPlusProxy *)p.second;
//     auto site = p.first;
//     // TODO: generelize
//     if (0 == site) {
//       FutureAttr fuattr;
//       fuattr.callback = [](Future *fu) {};

// #ifdef CURP_TIME_DEBUG
//       struct timeval tp;
//       gettimeofday(&tp, NULL);
//       Log_info("[CURP] [2-] [tx=%d] Forward %.3f", dynamic_pointer_cast<TpcCommitCommand>(cmd)->tx_id_, tp.tv_sec * 1000 + tp.tv_usec / 1000.0);
// #endif

//       Future *f = proxy->async_Forward(pos_deputy, cmd_deputy, accepted, fuattr);
//       Future::safe_release(f);
//     }
//   }
//   return e;
// }

// shared_ptr<CurpPlusCoordinatorAcceptQuorumEvent>
// CurpPlusCommo::BroadcastCoordinatorAccept(parid_t par_id,
//                           shared_ptr<Position> pos,
//                           shared_ptr<Marshallable> cmd) {
//   int n = Config::GetConfig()->GetPartitionSize(par_id);
//   auto e = Reactor::CreateSpEvent<CurpPlusCoordinatorAcceptQuorumEvent>(n);
//   auto proxies = rpc_par_proxies_[par_id];
//   MarshallDeputy pos_deputy(dynamic_pointer_cast<Marshallable>(pos)), cmd_deputy(cmd);
//   for (auto& p : proxies) {
//     auto proxy = (CurpPlusProxy *)p.second;
//     auto site = p.first;
//     FutureAttr fuattr;
//     fuattr.callback = [e](Future *fu) {
//       bool_t accepted;
//       fu->get_reply() >> accepted;
//       e->FeedResponse(accepted);
//     };
//     Future *f = proxy->async_CoordinatorAccept(pos_deputy, cmd_deputy, fuattr);
//     Future::safe_release(f);
//   }
//   return e;
// }

// shared_ptr<CurpPlusPrepareQuorumEvent>
// CurpPlusCommo::BroadcastPrepare(parid_t par_id,
//                   shared_ptr<Position> pos,
//                   ballot_t ballot) {
//   int n = Config::GetConfig()->GetPartitionSize(par_id);
//   auto e = Reactor::CreateSpEvent<CurpPlusPrepareQuorumEvent>(n);
//   auto proxies = rpc_par_proxies_[par_id];
//   MarshallDeputy pos_deputy(dynamic_pointer_cast<Marshallable>(pos));
//   for (auto& p : proxies) {
//     auto proxy = (CurpPlusProxy *)p.second;
//     auto site = p.first;
//     FutureAttr fuattr;
//     fuattr.callback = [e](Future *fu) {
//       bool_t accepted;
//       ballot_t seen_ballot;
//       i32 last_accepted_status;
//       MarshallDeputy last_accepted_cmd;
//       ballot_t last_accepted_ballot;
//       fu->get_reply() >> accepted >> seen_ballot >> last_accepted_status >> last_accepted_cmd >> last_accepted_ballot;
//       e->FeedResponse(accepted, seen_ballot, last_accepted_status, last_accepted_cmd, last_accepted_ballot);
//     };
//     Future *f = proxy->async_Prepare(pos_deputy, ballot, fuattr);
//     Future::safe_release(f);
//   }
//   return e;
// }

// shared_ptr<CurpPlusAcceptQuorumEvent>
// CurpPlusCommo::BroadcastAccept(parid_t par_id,
//                 shared_ptr<Position> pos,
//                 shared_ptr<Marshallable> cmd,
//                 ballot_t ballot) {
//   int n = Config::GetConfig()->GetPartitionSize(par_id);
//   auto e = Reactor::CreateSpEvent<CurpPlusAcceptQuorumEvent>(n);
//   auto proxies = rpc_par_proxies_[par_id];
//   MarshallDeputy pos_deputy(dynamic_pointer_cast<Marshallable>(pos)), cmd_deputy(cmd);
//   for (auto& p : proxies) {
//     auto proxy = (CurpPlusProxy *)p.second;
//     auto site = p.first;
//     FutureAttr fuattr;
//     fuattr.callback = [e](Future *fu) {
//       bool_t accepted;
//       ballot_t seen_ballot;
//       fu->get_reply() >> accepted >> seen_ballot;
//       e->FeedResponse(accepted, seen_ballot);
//     };
//     Future *f = proxy->async_Accept(pos_deputy, cmd_deputy, ballot, fuattr);
//     Future::safe_release(f);
//   }
//   return e;
// }

// shared_ptr<IntEvent>
// CurpPlusCommo::BroadcastCommit(parid_t par_id,
//                                 shared_ptr<Position> pos,
//                                 shared_ptr<Marshallable> cmd,
//                                 uint16_t ban_site) {
//   int n = Config::GetConfig()->GetPartitionSize(par_id);
//   auto e = Reactor::CreateSpEvent<IntEvent>();
//   auto proxies = rpc_par_proxies_[par_id];
//   MarshallDeputy pos_deputy(dynamic_pointer_cast<Marshallable>(pos)), cmd_deputy(cmd);
//   for (auto& p : proxies) {
//     auto proxy = (CurpPlusProxy *)p.second;
//     auto site = p.first;
//     if (site != ban_site) {
//       FutureAttr fuattr;
//       fuattr.callback = [](Future *fu) {};
//       Future *f = proxy->async_Commit(pos_deputy, cmd_deputy, fuattr);
//       Future::safe_release(f);
//     }
//   }
//   return e;
// }

} // namespace janus
