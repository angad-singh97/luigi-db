#include "curp_service.h"

namespace janus {

void CurpServiceImpl::CurpDispatch(const int32_t& client_id,
                                    const int32_t& cmd_id_in_client,
                                    const MarshallDeputy& cmd,
                                    bool_t* accepted,
                                    ver_t* ver,
                                    int32_t* result,
                                    siteid_t* coo_id,
                                    rrr::DeferredReply* defer) {
  verify(sched_ != nullptr);



#ifdef CURP_TIME_DEBUG
  struct timeval tp;
  gettimeofday(&tp, NULL);
  Log_info("[CURP] [1+] [tx=%d] on PoorDispatch %.3f", dynamic_pointer_cast<TpcCommitCommand>(const_cast<MarshallDeputy&>(cmd).sp_data_)->tx_id_, tp.tv_sec * 1000 + tp.tv_usec / 1000.0);
#endif

  // Log_info("[CURP] Received request from async_PoorDispatch");
#ifdef TC
  usleep(TC_LATENCY);
#endif
  // Log_info("[CURP] received CurpPoorDispatch");
  sched_->OnCurpDispatch(client_id,
                      cmd_id_in_client,
                      const_cast<MarshallDeputy&>(cmd).sp_data_,
                      accepted,
                      ver,
                      result,
                      coo_id,
                      bind(&rrr::DeferredReply::reply, defer));
}

void CurpServiceImpl::CurpWaitCommit(const int32_t& client_id,
                                      const int32_t& cmd_id_in_client,
                                      bool_t* committed,
                                      value_t* commit_result,
                                      rrr::DeferredReply* defer) {
  verify(sched_ != nullptr);
#ifdef TC
  usleep(TC_LATENCY);
#endif
  sched_->OnCurpWaitCommit(client_id,
                        cmd_id_in_client,
                        committed,
                        commit_result,
                        bind(&rrr::DeferredReply::reply, defer));
}

void CurpServiceImpl::CurpForward(const bool_t& accepted,
                                  const ver_t& ver,
                                  const MarshallDeputy& cmd,
                                  rrr::DeferredReply* defer) {
  verify(sched_ != nullptr);

#ifdef CURP_TIME_DEBUG
  struct timeval tp;
  gettimeofday(&tp, NULL);
  Log_info("[CURP] [2+] [tx=%d] on Forward %.3f", dynamic_pointer_cast<TpcCommitCommand>(const_cast<MarshallDeputy&>(cmd).sp_data_)->tx_id_, tp.tv_sec * 1000 + tp.tv_usec / 1000.0);
#endif
#ifdef TC
  usleep(TC_LATENCY);
#endif
  sched_->OnCurpForward(accepted,
                        ver,
                        const_cast<MarshallDeputy&>(cmd).sp_data_);
  defer->reply();
}

// void CurpServiceImpl::CurpCoordinatorAccept(const MarshallDeputy& pos,
//                                                   const MarshallDeputy& cmd,
//                                                   bool_t* accepted,
//                                                   rrr::DeferredReply* defer) {
//   verify(sched_ != nullptr);
// #ifdef TC
//   usleep(TC_LATENCY);
// #endif
//   sched_->OnCurpCoordinatorAccept(dynamic_pointer_cast<Position>(const_cast<MarshallDeputy&>(pos).sp_data_),
//                               const_cast<MarshallDeputy&>(cmd).sp_data_,
//                               accepted,
//                               bind(&rrr::DeferredReply::reply, defer));
// }

void CurpServiceImpl::CurpPrepare(const key_t& k,
                                  const ver_t& ver,
                                  const ballot_t& ballot,
                                  bool_t* accepted,
                                  rrr::i32* status,
                                  ballot_t* replied_ballot,
                                  MarshallDeputy* cmd,
                                  rrr::DeferredReply* defer) {
#ifdef TC
  usleep(TC_LATENCY);
#endif
  sched_->OnCurpPrepare(k,
                        ver,
                        ballot,
                        accepted,
                        status,
                        replied_ballot,
                        &const_cast<MarshallDeputy&>(*cmd).sp_data_,
                        bind(&rrr::DeferredReply::reply, defer));
}

void CurpServiceImpl::CurpAccept(const ver_t& ver,
                                  const ballot_t& ballot,
                                  const MarshallDeputy& md_cmd,
                                  bool_t* accepted,
                                  ballot_t* seen_ballot,
                                  rrr::DeferredReply* defer) {
#ifdef TC
  usleep(TC_LATENCY);
#endif
  sched_->OnCurpAccept(ver,
                        ballot,
                        const_cast<MarshallDeputy&>(md_cmd).sp_data_,
                        accepted,
                        seen_ballot,
                        bind(&rrr::DeferredReply::reply, defer));
}

void CurpServiceImpl::CurpCommit(const ver_t& ver,
            const MarshallDeputy& cmd,
            rrr::DeferredReply* defer) {
  verify(sched_ != nullptr);
  // Log_info("[CURP] CurpServiceImpl::CurpCommit site %d", sched_->site_id_);
#ifdef TC
  usleep(TC_LATENCY);
#endif
  sched_->OnCurpCommit(ver,
                        const_cast<MarshallDeputy&>(cmd).sp_data_);
  defer->reply();
}

// void CurpServiceImpl::OriginalSubmit(const MarshallDeputy& md,
//                                                 const rrr::i64& dep_id,
//                                                 bool_t* slow,
//                                                 rrr::DeferredReply* defer)  {
//   verify(sched_ != nullptr);
// #ifdef TC
//   usleep(TC_LATENCY);
// #endif
//   shared_ptr<Marshallable> cmd{md.sp_data_};
//   sched_->OnOriginalSubmit(cmd, 
//                             dep_id,
//                             slow,
//                             bind(&rrr::DeferredReply::reply, defer));
// }

void CurpServiceImpl::CurpTest(const int32_t& a,
                                          int32_t* b,
                                          rrr::DeferredReply* defer) {
  verify(a == 42);
#ifdef TC
  usleep(TC_LATENCY);
#endif
  Log_info("[CURP] received sent 42");
  *b = 24;
  defer->reply();
}

// void CurpServiceImpl::CurpProposeFinish(const int32_t& key,
//                                         uint64_t* pos,
//                                         rrr::DeferredReply* defer) {
//   verify(sched_ != nullptr);
//   sched_->OnCurpProposeFinish(key,
//                               pos,
//                               bind(&rrr::DeferredReply::reply, defer));
// }

// void CurpServiceImpl::CurpCommitFinish(const MarshallDeputy& pos,
//                                         rrr::DeferredReply* defer) {
//   verify(sched_ != nullptr);
//   sched_->OnCurpCommitFinish(dynamic_pointer_cast<Position>(const_cast<MarshallDeputy&>(pos).sp_data_),
//                              bind(&rrr::DeferredReply::reply, defer));
// }


};