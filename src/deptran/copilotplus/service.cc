#include "service.h"
#include "server.h"

namespace janus {

CopilotPlusServiceImpl::CopilotPlusServiceImpl(TxLogServer *sched) {
  sched_ = sched;
}

void CopilotPlusServiceImpl::Forward(const MarshallDeputy& cmd,
                                 rrr::DeferredReply* defer) {
  auto coro = Coroutine::CreateRun([&]() {
    sched()->OnForward(const_cast<MarshallDeputy&>(cmd).sp_data_,
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
  sched()->OnPrepare(is_pilot, slot,
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
  // Log_info("[copilot+] CopilotPlusServiceImpl::FastAccept");
  // auto coro = Coroutine::CreateRun([&]() {
    sched()->OnFastAccept(is_pilot, slot,
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
  // auto coro = Coroutine::CreateRun([&]() {
    sched()->OnAccept(is_pilot, slot,
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
  // Coroutine::CreateRun([&]() {
    sched()->OnCommit(is_pilot, slot, dep,
                     const_cast<MarshallDeputy&>(cmd).sp_data_);
    defer->reply();
  // });
}


} // namespace janus
