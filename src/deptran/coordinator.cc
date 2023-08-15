#include "marshal-value.h"
#include "coordinator.h"
#include "frame.h"
#include "constants.h"
#include "sharding.h"
#include "workload.h"
#include "benchmark_control_rpc.h"

/**
 * What shoud we do to change this to asynchronous?
 * 1. Fisrt we need to have a queue to hold all transaction requests.
 * 2. pop next request, send start request for each piece until there is no
 *available left.
 *          in the callback, send the next piece of start request.
 *          if responses to all start requests are collected.
 *              send the finish request
 *                  in callback, remove it from queue.
 *
 *
 */

namespace janus {
std::mutex Coordinator::_dbg_txid_lock_{};
std::unordered_set<txid_t> Coordinator::_dbg_txid_set_{};

Coordinator::Coordinator(uint32_t coo_id,
                         int32_t benchmark,
                         ClientControlServiceImpl *ccsi,
                         uint32_t thread_id) : coo_id_(coo_id),
                                               benchmark_(benchmark),
                                               ccsi_(ccsi),
                                               thread_id_(thread_id),
                                               mtx_() {
  uint64_t k = coo_id_;
  k <<= 32;
  k++;
  this->next_pie_id_.store(k);
  this->next_txn_id_.store(k);
  recorder_ = NULL;
  retry_wait_ = Config::GetConfig()->retry_wait();

	struct timespec begin, end;
	//clock_gettime(CLOCK_MONOTONIC, &begin);
  
	// TODO this would be slow.
  vector<string> addrs;
  Config::GetConfig()->get_all_site_addr(addrs);
//  Log_info("Initializing site_prepare_ for %x: %p", this, site_prepare_);
  site_prepare_.resize(addrs.size(), 0);
  // Log_info("What is the first value of site_prepare_ for %x: %d", this, site_prepare_[0]);
  site_commit_.resize(addrs.size(), 0);
  site_abort_.resize(addrs.size(), 0);
  site_piece_.resize(addrs.size(), 0);
	
	/*clock_gettime(CLOCK_MONOTONIC, &end);
	Log_info("time of 2nd part of CreateCoordinator: %d", end.tv_nsec-begin.tv_nsec);*/
}

Coordinator::~Coordinator() {
//  for (int i = 0; i < site_prepare_.size(); i++) {
//    Log_debug("Coo: %u, Site: %d, accept: %d, "
//                 "prepare: %d, commit: %d, abort: %d",
//             coo_id_, i, site_piece_[i], site_prepare_[i],
//             site_commit_[i], site_abort_[i]);
//  }

  if (recorder_) delete recorder_;
#ifdef TXN_STAT

  for (auto& it : txn_stats_) {
        Log::info("TXN: %d", it.first);
        it.second.output();
      }
#endif /* ifdef TXN_STAT */

  // debug;
  mtx_.lock();
  mtx_.unlock();
// TODO (shuai) destroy all the rpc clients and proxies.
}

// // [CURP] TODO: rm this
// void Coordinator::CurpSubmit(shared_ptr<Marshallable>& cmd,
//                       const std::function<void()>& commit_callback,
//                       const std::function<void()>& exe_callback) {
//   // verify(0);
//   std::lock_guard<std::recursive_mutex> lock(mtx_);
//   verify(commo_);
//   // Log_info("[CURP] enter CoordinatorCurpPlus::Submit!!!!!!!!!!!!!!!!!!!!");
//   // verify(sch_ != nullptr);
//   // Log_info("[CURP] the coresponding server is %d %d", ((CurpPlusServer*) sch_)->loc_id_, ((CurpPlusServer*) sch_)->site_id_);

//   // return;

//   // shared_ptr<IntEvent> test_event = commo()->BroadcastTest();
//   // test_event->Wait();
//   // Log_info("[CURP] Passed Curp Test");

//   shared_ptr<CurpDispatchQuorumEvent> sq_quorum = commo_->CurpBroadcastDispatch(cmd);
//   commit_callback_ = commit_callback;

//   sq_quorum->Wait();

//   // shared_ptr<TpcCommitCommand> tpc_cmd = dynamic_pointer_cast<TpcCommitCommand>(cmd);
//   // VecPieceData *cmd_cast = (VecPieceData*)(tpc_cmd->cmd_.get());
//   // shared_ptr<vector<shared_ptr<SimpleCommand>>> sp_vec_piece = cmd_cast->sp_vec_piece_data_;
//   // shared_ptr<TxPieceData> simple_cmd = *(sp_vec_piece->begin());
//   // Log_info("Cmd(%d, %d) quorum %s result %d", simple_cmd->client_id_, simple_cmd->cmd_id_in_client_, sq_quorum->Print().c_str(), sq_quorum->FastYes());
  
//   // Log_info("[CURP] After quorum");
//   if (sq_quorum->FastYes()) {
//     // Fastpath Success
//     // Log_info("[CURP] Fastpath Success");
//     // [CURP] TODO: maybe some problem
//     commit_callback_();
//   } else if (sq_quorum->FastNo() || sq_quorum->timeouted_) {
//     // Log_info("[CURP] Fail FastYes path");
//     // Fastpath timeout
//     // Log_info("[CURP] Fastpath Fail");
//     auto wait_quorum = commo_->CurpBroadcastWaitCommit(cmd, sq_quorum->GetCooId());
//     wait_quorum->Wait();
//     if (wait_quorum->Yes()) {// 0 fail 1 success
//       commit_callback_();
//     } 
//     else if (wait_quorum->No()) {
//       commo_->OriginalDispatch(cmd, frame_->site_info_->id, dep_id_);
//       commit_callback_();
//     }
//   } else {
//     verify(0);
//   }
// }

// void Coordinator::OriginalSubmit(shared_ptr<Marshallable>& cmd,
//                     const std::function<void()>& commit_callback,
//                     const std::function<void()>& exe_callback) {
//   verify(commo_);
//   commo_->OriginalDispatch(cmd, frame_->site_info_->id, dep_id_);
//   commit_callback_ = commit_callback;
// }

} // namespace janus
