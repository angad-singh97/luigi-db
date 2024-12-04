
#include "service.h"
#include "server.h"
#include "../RW_command.h"

namespace janus {

MenciusPlusServiceImpl::MenciusPlusServiceImpl(TxLogServer *sched) {
  sched_ = sched;
}

void MenciusPlusServiceImpl::Prepare(const uint64_t& slot,
                                    const ballot_t& ballot,
                                    ballot_t* max_ballot,
                                    uint64_t* coro_id,
                                    rrr::DeferredReply* defer) {
  sched()->OnPrepare(slot,
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
                                   const uint64_t& commit_finish,
                                   ballot_t* max_ballot,
                                   uint64_t* coro_id,
                                   bool_t* finish_accept,
                                   uint64_t* finish_ver,
                                   rrr::DeferredReply* defer) {
  verify(sched_ != nullptr);
  // auto start = chrono::system_clock::now();

  // time_t tstart = chrono::system_clock::to_time_t(start);
  // tm * date = localtime(&tstart);
  // date->tm_hour = 0;
  // date->tm_min = 0;
  // date->tm_sec = 0;
  // auto midn = chrono::system_clock::from_time_t(std::mktime(date));

  // auto hours = chrono::duration_cast<chrono::hours>(start-midn);
  // auto minutes = chrono::duration_cast<chrono::minutes>(start-midn);
  // auto seconds = chrono::duration_cast<chrono::seconds>(start-midn);

  // auto start_ = chrono::duration_cast<chrono::microseconds>(start-midn-hours-minutes).count();
  // Log_info("Duration of RPC is: %d", start_-time);

  // the only case for the current slot is SKIP or current value
  //SimpleRWCommand parsed_cmd = SimpleRWCommand(md_cmd.sp_data_);
  //sched_->c_mutex.lock();
  //sched_->unexecuted_keys_[parsed_cmd.key_] += 1;
  //sched_->c_mutex.unlock();

  // sched_->g_mutex.lock();
  // // update the received potential SKIPs
  // int n = Config::GetConfig()->GetPartitionSize(sched_->partition_id_);
  // if (skip_potentials.size()>100) {
  //   sched_->skip_potentials_recd[(slot-1)%n].clear();
  //   for (auto x: skip_potentials){
  //     sched_->skip_potentials_recd[(slot-1)%n].insert(x);
  //   }
  // }
  
  // // commit the SKIP
  // for (auto x: skip_commits){
  //   auto cmd = std::make_shared<TpcCommitCommand>();
  //   MarshallDeputy md(cmd);
  //   md.kind_ = MarshallDeputy::CMD_TPC_COMMIT;
  //   sched_->OnCommit(x, 100, md.sp_data_, true);
  // }
  // sched_->g_mutex.unlock();

  auto coro = Coroutine::CreateRun([&] () {
    sched()->OnSuggest(slot,
		                 time,
                     ballot,
                     sender,
                     skip_commits,
                     skip_potentials,
                     const_cast<MarshallDeputy&>(md_cmd).sp_data_,
                     commit_finish,
                     max_ballot,
                     coro_id,
                     finish_accept,
                     finish_ver,
                     std::bind(&rrr::DeferredReply::reply, defer));

  });

  auto end = chrono::system_clock::now();
  //auto duration = chrono::duration_cast<chrono::microseconds>(end-start);
  //Log_info("Duration of Suggest() at Follower's side is: %d", duration.count());
  //Log_info("coro id on service side: %d", coro->id);
}

void MenciusPlusServiceImpl::Decide(const uint64_t& slot,
                                   const ballot_t& ballot,
                                   const MarshallDeputy& md_cmd,
                                   rrr::DeferredReply* defer) {
  auto x = md_cmd.sp_data_;
  SimpleRWCommand parsed_cmd = SimpleRWCommand(md_cmd.sp_data_);
  sched()->c_mutex.lock();
  sched()->unexecuted_keys_[parsed_cmd.key_] -= 1;
  assert(sched()->unexecuted_keys_[parsed_cmd.key_]>=0);
  sched()->c_mutex.unlock();
  sched()->OnCommit(slot, ballot,x);
  defer->reply();
}

} // namespace janus;
