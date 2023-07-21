
#include "service.h"
#include "server.h"

namespace janus {

CurpPlusServiceImpl::CurpPlusServiceImpl(TxLogServer *sched)
    : sched_((CurpPlusServer*)sched) {

}

// void CurpPlusServiceImpl::Dispatch(const int32_t& client_id,
//                                     const int32_t& cmd_id_in_client,
//                                     const MarshallDeputy& cmd,
//                                     bool_t* accepted,
//                                     MarshallDeputy* pos,
//                                     int32_t* result,
//                                     siteid_t* coo_id,
//                                     rrr::DeferredReply* defer) {
//   verify(sched_ != nullptr);
//   sched_->OnDispatch(client_id,
//                       cmd_id_in_client,
//                       const_cast<MarshallDeputy&>(cmd).sp_data_,
//                       accepted,
//                       pos,
//                       result,
//                       coo_id,
//                       bind(&rrr::DeferredReply::reply, defer));
// }

// void CurpPlusServiceImpl::PoorDispatch(const int32_t& client_id,
//                                     const int32_t& cmd_id_in_client,
//                                     const MarshallDeputy& cmd,
//                                     bool_t* accepted,
//                                     pos_t* pos0,
//                                     pos_t* pos1,
//                                     int32_t* result,
//                                     siteid_t* coo_id,
//                                     rrr::DeferredReply* defer) {
//   verify(sched_ != nullptr);

// #ifdef CURP_TIME_DEBUG
//   struct timeval tp;
//   gettimeofday(&tp, NULL);
//   Log_info("[CURP] [1+] [tx=%d] on PoorDispatch %.3f", dynamic_pointer_cast<TpcCommitCommand>(const_cast<MarshallDeputy&>(cmd).sp_data_)->tx_id_, tp.tv_sec * 1000 + tp.tv_usec / 1000.0);
// #endif

//   Log_info("[CURP] Received request from async_PoorDispatch");

//   sched_->OnPoorDispatch(client_id,
//                       cmd_id_in_client,
//                       const_cast<MarshallDeputy&>(cmd).sp_data_,
//                       accepted,
//                       pos0,
//                       pos1,
//                       result,
//                       coo_id,
//                       bind(&rrr::DeferredReply::reply, defer));
// }

// void CurpPlusServiceImpl::Test(const int32_t& a,
//             int32_t* b,
//             rrr::DeferredReply* defer)  {
//   verify(a == 42);
//   Log_info("[CURP] Server Received 42");
//   *b = 24;
//   defer->reply();
// }

// void CurpPlusServiceImpl::WaitCommit(const int32_t& client_id,
//                                       const int32_t& cmd_id_in_client,
//                                       bool_t* committed,
//                                       rrr::DeferredReply* defer) {
//   verify(sched_ != nullptr);
//   sched_->OnWaitCommit(client_id,
//                         cmd_id_in_client,
//                         committed,
//                         bind(&rrr::DeferredReply::reply, defer));
// }

// void CurpPlusServiceImpl::Forward(const MarshallDeputy& pos,
//                                         const MarshallDeputy& cmd,
//                                         const bool_t& accepted,
//                                         rrr::DeferredReply* defer) {
//   verify(sched_ != nullptr);

// #ifdef CURP_TIME_DEBUG
//   struct timeval tp;
//   gettimeofday(&tp, NULL);
//   Log_info("[CURP] [2+] [tx=%d] on Forward %.3f", dynamic_pointer_cast<TpcCommitCommand>(const_cast<MarshallDeputy&>(cmd).sp_data_)->tx_id_, tp.tv_sec * 1000 + tp.tv_usec / 1000.0);
// #endif

//   sched_->OnForward(dynamic_pointer_cast<Position>(const_cast<MarshallDeputy&>(pos).sp_data_),
//                     const_cast<MarshallDeputy&>(cmd).sp_data_,
//                     accepted);
//   defer->reply();
// }

// void CurpPlusServiceImpl::CoordinatorAccept(const MarshallDeputy& pos,
//                                                   const MarshallDeputy& cmd,
//                                                   bool_t* accepted,
//                                                   rrr::DeferredReply* defer) {
//   verify(sched_ != nullptr);

//   sched_->OnCoordinatorAccept(dynamic_pointer_cast<Position>(const_cast<MarshallDeputy&>(pos).sp_data_),
//                               const_cast<MarshallDeputy&>(cmd).sp_data_,
//                               accepted,
//                               bind(&rrr::DeferredReply::reply, defer));
// }

// void CurpPlusServiceImpl::Prepare(const MarshallDeputy& pos,
//             const ballot_t& ballot,
//             bool_t* accepted,
//             ballot_t* seen_ballot,
//             rrr::i32* last_accepted_status,
//             MarshallDeputy* last_accepted_cmd,
//             ballot_t* last_accepted_ballot,
//             rrr::DeferredReply* defer) {
//   verify(sched_ != nullptr);
//   // TODO: correct for last_accepted_cmd?
//   sched_->OnPrepare(dynamic_pointer_cast<Position>(const_cast<MarshallDeputy&>(pos).sp_data_),
//                     ballot,
//                     accepted,
//                     seen_ballot,
//                     last_accepted_status,
//                     &const_cast<MarshallDeputy&>(*last_accepted_cmd).sp_data_,
//                     last_accepted_ballot,
//                     bind(&rrr::DeferredReply::reply, defer));
// }

// void CurpPlusServiceImpl::Accept(const MarshallDeputy& pos,
//             const MarshallDeputy& cmd,
//             const ballot_t& ballot,
//             bool_t* accepted,
//             ballot_t* seen_ballot,
//             rrr::DeferredReply* defer) {
//   verify(sched_ != nullptr);
//   sched_->OnAccept(dynamic_pointer_cast<Position>(const_cast<MarshallDeputy&>(pos).sp_data_),
//                     const_cast<MarshallDeputy&>(cmd).sp_data_,
//                     ballot,
//                     accepted,
//                     seen_ballot,
//                     bind(&rrr::DeferredReply::reply, defer));
// }

// void CurpPlusServiceImpl::Commit(const MarshallDeputy& pos,
//             const MarshallDeputy& cmd,
//             rrr::DeferredReply* defer) {
//   verify(sched_ != nullptr);
//   sched_->OnCommit(dynamic_pointer_cast<Position>(const_cast<MarshallDeputy&>(pos).sp_data_),
//                     const_cast<MarshallDeputy&>(cmd).sp_data_);
//   defer->reply();
// }


} // namespace janus;
