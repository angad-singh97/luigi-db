
#include "service.h"
#include "server.h"
#include "../RW_command.h"

namespace janus {

MenciusPlusServiceImpl::MenciusPlusServiceImpl(TxLogServer *sched)
    : sched_((MenciusPlusServer*)sched) {

}

void MenciusPlusServiceImpl::Prepare(const uint64_t& slot,
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

void MenciusPlusServiceImpl::Suggest(const uint64_t& slot,
		                               const uint64_t& time,
                                   const ballot_t& ballot,
                                   const uint64_t& sender,
                                   const std::vector<uint64_t>& skip_commits, 
                                   const std::vector<uint64_t>& skip_potentials,
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

  // the only case for the current slot is SKIP or current value
  SimpleRWCommand parsed_cmd = SimpleRWCommand(md_cmd.sp_data_);
  sched_->c_mutex.lock();
  sched_->uncommitted_keys_[parsed_cmd.key_] += 1;
  sched_->c_mutex.unlock();

  sched_->g_mutex.lock();
  // update the received potential SKIPs
  int n = Config::GetConfig()->GetPartitionSize(sched_->partition_id_);
  sched_->skip_potentials_recd[(slot-1)%n].clear();
  for (auto x: skip_potentials){
    sched_->skip_potentials_recd[(slot-1)%n].insert(x);
  }
  
  // commit the SKIP
  for (auto x: skip_commits){
    auto cmd = std::make_shared<TpcCommitCommand>();
    MarshallDeputy md(cmd);
    md.kind_ = MarshallDeputy::CMD_TPC_COMMIT;
    sched_->OnCommit(x, 100, md.sp_data_, true);
  }
  sched_->g_mutex.unlock();

  auto coro = Coroutine::CreateRun([&] () {
    sched_->OnSuggest(slot,
		                 time,
                     ballot,
                     sender,
                     skip_commits,
                     skip_potentials,
                     const_cast<MarshallDeputy&>(md_cmd).sp_data_,
                     max_ballot,
                     coro_id,
                     std::bind(&rrr::DeferredReply::reply, defer));

  });

  auto end = chrono::system_clock::now();
  auto duration = chrono::duration_cast<chrono::microseconds>(end-start);
  //Log_info("Duration of Suggest() at Follower's side is: %d", duration.count());
  //Log_info("coro id on service side: %d", coro->id);
}

void MenciusPlusServiceImpl::Decide(const uint64_t& slot,
                                   const ballot_t& ballot,
                                   const MarshallDeputy& md_cmd,
                                   rrr::DeferredReply* defer) {
  verify(sched_ != nullptr);
  auto x = md_cmd.sp_data_;
  SimpleRWCommand parsed_cmd = SimpleRWCommand(md_cmd.sp_data_);
  sched_->c_mutex.lock();
  sched_->uncommitted_keys_[parsed_cmd.key_] -= 1;
  assert(sched_->uncommitted_keys_[parsed_cmd.key_]>=0);
  sched_->c_mutex.unlock();
  sched_->OnCommit(slot, ballot,x);
  defer->reply();
}

// below are about CURP

void MenciusPlusServiceImpl::CurpPoorDispatch(const int32_t& client_id,
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

void MenciusPlusServiceImpl::CurpWaitCommit(const int32_t& client_id,
                                      const int32_t& cmd_id_in_client,
                                      bool_t* committed,
                                      rrr::DeferredReply* defer) {
  verify(sched_ != nullptr);
  sched_->OnCurpWaitCommit(client_id,
                        cmd_id_in_client,
                        committed,
                        bind(&rrr::DeferredReply::reply, defer));
}

void MenciusPlusServiceImpl::CurpForward(const MarshallDeputy& pos,
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

void MenciusPlusServiceImpl::CurpCoordinatorAccept(const MarshallDeputy& pos,
                                                  const MarshallDeputy& cmd,
                                                  bool_t* accepted,
                                                  rrr::DeferredReply* defer) {
  verify(sched_ != nullptr);

  sched_->OnCurpCoordinatorAccept(dynamic_pointer_cast<Position>(const_cast<MarshallDeputy&>(pos).sp_data_),
                              const_cast<MarshallDeputy&>(cmd).sp_data_,
                              accepted,
                              bind(&rrr::DeferredReply::reply, defer));
}

void MenciusPlusServiceImpl::CurpPrepare(const MarshallDeputy& pos,
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

void MenciusPlusServiceImpl::CurpAccept(const MarshallDeputy& pos,
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

void MenciusPlusServiceImpl::CurpCommit(const MarshallDeputy& pos,
            const MarshallDeputy& cmd,
            rrr::DeferredReply* defer) {
  verify(sched_ != nullptr);
  // Log_info("[CURP] MenciusPlusServiceImpl::CurpCommit site %d", sched_->site_id_);
  sched_->OnCurpCommit(dynamic_pointer_cast<Position>(const_cast<MarshallDeputy&>(pos).sp_data_),
                    const_cast<MarshallDeputy&>(cmd).sp_data_);
  defer->reply();
}

} // namespace janus;
