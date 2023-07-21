#include "service.h"
#include "server.h"

namespace janus {

CopilotPlusServiceImpl::CopilotPlusServiceImpl(TxLogServer *sched)
    : sched_((CopilotPlusServer *)sched) {
}

void CopilotPlusServiceImpl::Forward(const MarshallDeputy& cmd,
                                 rrr::DeferredReply* defer) {
  verify(sched_);
  auto coro = Coroutine::CreateRun([&]() {
    sched_->OnForward(const_cast<MarshallDeputy&>(cmd).sp_data_,
                      std::bind(&rrr::DeferredReply::reply, defer));
  });
}

void CopilotPlusServiceImpl::Prepare(const uint8_t& is_pilot,
                                 const uint64_t& slot,
                                 const ballot_t& ballot,
                                 const struct DepId& dep_id,
                                 MarshallDeputy* ret_cmd,
                                 ballot_t* max_ballot,
                                 uint64_t* dep,
                                 status_t* status,
                                 rrr::DeferredReply* defer) {
  verify(sched_);
  sched_->OnPrepare(is_pilot, slot,
                    ballot,
                    dep_id,
                    ret_cmd,
                    max_ballot,
                    dep,
                    status,
                    bind(&rrr::DeferredReply::reply, defer));
}

void CopilotPlusServiceImpl::FastAccept(const uint8_t& is_pilot,
                                    const uint64_t& slot,
                                    const ballot_t& ballot,
                                    const uint64_t& dep,
                                    const MarshallDeputy& cmd,
                                    const struct DepId& dep_id,
                                    ballot_t* max_ballot,
                                    uint64_t* ret_dep,
                                    rrr::DeferredReply* defer) {
  verify(sched_);
  // Log_info("[copilot+] CopilotPlusServiceImpl::FastAccept");
  // auto coro = Coroutine::CreateRun([&]() {
    sched_->OnFastAccept(is_pilot, slot,
                         ballot,
                         dep,
                         const_cast<MarshallDeputy&>(cmd).sp_data_,
                         dep_id,
                         max_ballot,
                         ret_dep,
                         bind(&rrr::DeferredReply::reply, defer));
  // });
}

void CopilotPlusServiceImpl::Accept(const uint8_t& is_pilot,
                                const uint64_t& slot,
                                const ballot_t& ballot,
                                const uint64_t& dep,
                                const MarshallDeputy& cmd,
                                const struct DepId& dep_id,
                                ballot_t* max_ballot,
                                rrr::DeferredReply* defer) {
  verify(sched_);

  // auto coro = Coroutine::CreateRun([&]() {
    sched_->OnAccept(is_pilot, slot,
                     ballot,
                     dep,
                     const_cast<MarshallDeputy&>(cmd).sp_data_,
                     dep_id,
                     max_ballot,
                     bind(&rrr::DeferredReply::reply, defer));
  // });
}

void CopilotPlusServiceImpl::Commit(const uint8_t& is_pilot,
                                const uint64_t& slot,
                                const uint64_t& dep,
                                const MarshallDeputy& cmd,
                                rrr::DeferredReply* defer) {
  verify(sched_);
  // Coroutine::CreateRun([&]() {
    sched_->OnCommit(is_pilot, slot, dep,
                     const_cast<MarshallDeputy&>(cmd).sp_data_);
    defer->reply();
  // });
}

// below are about CURP

void CopilotPlusServiceImpl::CurpPoorDispatch(const int32_t& client_id,
                                    const int32_t& cmd_id_in_client,
                                    const MarshallDeputy& cmd,
                                    bool_t* accepted,
                                    pos_t* pos0,
                                    pos_t* pos1,
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

  sched_->OnCurpPoorDispatch(client_id,
                      cmd_id_in_client,
                      const_cast<MarshallDeputy&>(cmd).sp_data_,
                      accepted,
                      pos0,
                      pos1,
                      result,
                      coo_id,
                      bind(&rrr::DeferredReply::reply, defer));
}

void CopilotPlusServiceImpl::CurpWaitCommit(const int32_t& client_id,
                                      const int32_t& cmd_id_in_client,
                                      bool_t* committed,
                                      rrr::DeferredReply* defer) {
  verify(sched_ != nullptr);
  sched_->OnCurpWaitCommit(client_id,
                        cmd_id_in_client,
                        committed,
                        bind(&rrr::DeferredReply::reply, defer));
}

void CopilotPlusServiceImpl::CurpForward(const MarshallDeputy& pos,
                                        const MarshallDeputy& cmd,
                                        const bool_t& accepted,
                                        rrr::DeferredReply* defer) {
  verify(sched_ != nullptr);

#ifdef CURP_TIME_DEBUG
  struct timeval tp;
  gettimeofday(&tp, NULL);
  Log_info("[CURP] [2+] [tx=%d] on Forward %.3f", dynamic_pointer_cast<TpcCommitCommand>(const_cast<MarshallDeputy&>(cmd).sp_data_)->tx_id_, tp.tv_sec * 1000 + tp.tv_usec / 1000.0);
#endif

  sched_->OnCurpForward(dynamic_pointer_cast<Position>(const_cast<MarshallDeputy&>(pos).sp_data_),
                    const_cast<MarshallDeputy&>(cmd).sp_data_,
                    accepted);
  defer->reply();
}

void CopilotPlusServiceImpl::CurpCoordinatorAccept(const MarshallDeputy& pos,
                                                  const MarshallDeputy& cmd,
                                                  bool_t* accepted,
                                                  rrr::DeferredReply* defer) {
  verify(sched_ != nullptr);

  sched_->OnCurpCoordinatorAccept(dynamic_pointer_cast<Position>(const_cast<MarshallDeputy&>(pos).sp_data_),
                              const_cast<MarshallDeputy&>(cmd).sp_data_,
                              accepted,
                              bind(&rrr::DeferredReply::reply, defer));
}

void CopilotPlusServiceImpl::CurpPrepare(const MarshallDeputy& pos,
            const ballot_t& ballot,
            bool_t* accepted,
            ballot_t* seen_ballot,
            rrr::i32* last_accepted_status,
            MarshallDeputy* last_accepted_cmd,
            ballot_t* last_accepted_ballot,
            rrr::DeferredReply* defer) {
  verify(sched_ != nullptr);
  // TODO: correct for last_accepted_cmd?
  sched_->OnCurpPrepare(dynamic_pointer_cast<Position>(const_cast<MarshallDeputy&>(pos).sp_data_),
                    ballot,
                    accepted,
                    seen_ballot,
                    last_accepted_status,
                    &const_cast<MarshallDeputy&>(*last_accepted_cmd).sp_data_,
                    last_accepted_ballot,
                    bind(&rrr::DeferredReply::reply, defer));
}

void CopilotPlusServiceImpl::CurpAccept(const MarshallDeputy& pos,
            const MarshallDeputy& cmd,
            const ballot_t& ballot,
            bool_t* accepted,
            ballot_t* seen_ballot,
            rrr::DeferredReply* defer) {
  verify(sched_ != nullptr);
  sched_->OnCurpAccept(dynamic_pointer_cast<Position>(const_cast<MarshallDeputy&>(pos).sp_data_),
                    const_cast<MarshallDeputy&>(cmd).sp_data_,
                    ballot,
                    accepted,
                    seen_ballot,
                    bind(&rrr::DeferredReply::reply, defer));
}

void CopilotPlusServiceImpl::CurpCommit(const MarshallDeputy& pos,
            const MarshallDeputy& cmd,
            rrr::DeferredReply* defer) {
  verify(sched_ != nullptr);
  // Log_info("[CURP] CopilotPlusServiceImpl::CurpCommit site %d", sched_->site_id_);
  sched_->OnCurpCommit(dynamic_pointer_cast<Position>(const_cast<MarshallDeputy&>(pos).sp_data_),
                    const_cast<MarshallDeputy&>(cmd).sp_data_);
  defer->reply();
}

} // namespace janus
