
#include "service.h"
#include "server.h"

namespace janus {

MultiPaxosPlusServiceImpl::MultiPaxosPlusServiceImpl(TxLogServer *sched)
    : sched_((PaxosPlusServer*)sched) {

}

void MultiPaxosPlusServiceImpl::Forward(const MarshallDeputy& md_cmd,
                                    const uint64_t& dep_id,
                                    uint64_t* coro_id,
                                    rrr::DeferredReply* defer) {
  verify(sched_ != nullptr);
  auto coro = Coroutine::CreateRun([&] () {
    sched_->OnForward(const_cast<MarshallDeputy&>(md_cmd).sp_data_,
                      dep_id,
                      coro_id,
                      std::bind(&rrr::DeferredReply::reply, defer));
  });
}

void MultiPaxosPlusServiceImpl::Prepare(const uint64_t& slot,
                                    const ballot_t& ballot,
                                    ballot_t* max_ballot,
                                    uint64_t* coro_id,
                                    rrr::DeferredReply* defer) {
  verify(sched_ != nullptr);
  sched_->OnPrepare(slot,
                    ballot,
                    max_ballot,
                    coro_id,
                    std::bind(&rrr::DeferredReply::reply, defer));
}

void MultiPaxosPlusServiceImpl::Accept(const uint64_t& slot,
		                   const uint64_t& time,
                                   const ballot_t& ballot,
                                   const MarshallDeputy& md_cmd,
                                   ballot_t* max_ballot,
                                   uint64_t* coro_id,
                                   rrr::DeferredReply* defer) {
  verify(sched_ != nullptr);
  auto start = chrono::system_clock::now();

  time_t tstart = chrono::system_clock::to_time_t(start);
  tm * date = localtime(&tstart);
  date->tm_hour = 0;
  date->tm_min = 0;
  date->tm_sec = 0;
  auto midn = chrono::system_clock::from_time_t(std::mktime(date));

  auto hours = chrono::duration_cast<chrono::hours>(start-midn);
  auto minutes = chrono::duration_cast<chrono::minutes>(start-midn);
  auto seconds = chrono::duration_cast<chrono::seconds>(start-midn);

  auto start_ = chrono::duration_cast<chrono::microseconds>(start-midn-hours-minutes).count();
  //Log_info("Duration of RPC is: %d", start_-time);

  auto coro = Coroutine::CreateRun([&] () {
    sched_->OnAccept(slot,
		     time,
                     ballot,
                     const_cast<MarshallDeputy&>(md_cmd).sp_data_,
                     max_ballot,
                     coro_id,
                     std::bind(&rrr::DeferredReply::reply, defer));

  });

  auto end = chrono::system_clock::now();
  auto duration = chrono::duration_cast<chrono::microseconds>(end-start);
  //Log_info("Duration of Accept() at Follower's side is: %d", duration.count());
  //Log_info("coro id on service side: %d", coro->id);
}

void MultiPaxosPlusServiceImpl::Decide(const uint64_t& slot,
                                   const ballot_t& ballot,
                                   const MarshallDeputy& md_cmd,
                                   rrr::DeferredReply* defer) {
  verify(sched_ != nullptr);
  auto x = md_cmd.sp_data_;
  sched_->OnCommit(slot, ballot,x);
  defer->reply();
}

// below are about CURP

void MultiPaxosPlusServiceImpl::CurpPoorDispatch(const int32_t& client_id,
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

void MultiPaxosPlusServiceImpl::CurpWaitCommit(const int32_t& client_id,
                                      const int32_t& cmd_id_in_client,
                                      bool_t* committed,
                                      rrr::DeferredReply* defer) {
  verify(sched_ != nullptr);
  sched_->OnCurpWaitCommit(client_id,
                        cmd_id_in_client,
                        committed,
                        bind(&rrr::DeferredReply::reply, defer));
}

void MultiPaxosPlusServiceImpl::CurpForward(const MarshallDeputy& pos,
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

void MultiPaxosPlusServiceImpl::CurpCoordinatorAccept(const MarshallDeputy& pos,
                                                  const MarshallDeputy& cmd,
                                                  bool_t* accepted,
                                                  rrr::DeferredReply* defer) {
  verify(sched_ != nullptr);

  sched_->OnCurpCoordinatorAccept(dynamic_pointer_cast<Position>(const_cast<MarshallDeputy&>(pos).sp_data_),
                              const_cast<MarshallDeputy&>(cmd).sp_data_,
                              accepted,
                              bind(&rrr::DeferredReply::reply, defer));
}

void MultiPaxosPlusServiceImpl::CurpPrepare(const MarshallDeputy& pos,
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

void MultiPaxosPlusServiceImpl::CurpAccept(const MarshallDeputy& pos,
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

void MultiPaxosPlusServiceImpl::CurpCommit(const MarshallDeputy& pos,
            const MarshallDeputy& cmd,
            rrr::DeferredReply* defer) {
  verify(sched_ != nullptr);
  // Log_info("[CURP] MultiPaxosPlusServiceImpl::CurpCommit site %d", sched_->site_id_);
  sched_->OnCurpCommit(dynamic_pointer_cast<Position>(const_cast<MarshallDeputy&>(pos).sp_data_),
                    const_cast<MarshallDeputy&>(cmd).sp_data_);
  defer->reply();
}

void MultiPaxosPlusServiceImpl::OriginalSubmit(const MarshallDeputy& md,
                                                const rrr::i64& dep_id,
                                                bool_t* slow,
                                                rrr::DeferredReply* defer)  {
  verify(sched_ != nullptr);
  shared_ptr<Marshallable> cmd{md.sp_data_};
  sched_->OnOriginalSubmit(cmd, 
                            dep_id,
                            slow,
                            bind(&rrr::DeferredReply::reply, defer));
}

} // namespace janus;
