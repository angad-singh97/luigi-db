#include "coordinator.h"
#include "frame.h"
#include "benchmark_control_rpc.h"
#include "../RW_command.h"

namespace janus {

// This Coordinator should be on Client Side

CoordinatorCurp::CoordinatorCurp(uint32_t coo_id,
                                       int32_t benchmark,
                                       ClientControlServiceImpl *ccsi,
                                       uint32_t thread_id)
  : CoordinatorClassic(coo_id, benchmark, ccsi, thread_id) {
  // Log_info("[CURP] CoordinatorCurp created for coo_id=%d thread_id=%d", coo_id, thread_id);
}

void CoordinatorCurp::GotoNextPhase() {
  int n_phase = 4;
  int current_phase = phase_ % n_phase;
  switch (phase_++ % n_phase) {
    case Phase::INIT_END:
      verify(phase_ % n_phase == Phase::DISPATCH);
      dispatch_time_ = SimpleRWCommand::GetCurrentMsTime();
      BroadcastDispatch();
      break;
    case Phase::DISPATCH:
      verify(phase_ % n_phase == Phase::QUERY);
      if (fast_path_success_) {
#ifdef CURP_FULL_LOG_DEBUG
        Log_info("[CURP] coo_id=%d cmd<%d, %d> fastpath success", coo_id_, SimpleRWCommand::GetCmdID(sp_vpd_).first, SimpleRWCommand::GetCmdID(sp_vpd_).second);
#endif
        fast_path_success_ = false;
        committed_ = true;
        phase_ += 2;
        verify(phase_ % n_phase == Phase::INIT_END);
        fastpath_count_++;
        cli2cli_[0].append(SimpleRWCommand::GetCurrentMsTime() - dispatch_time_);
        End();
      } else if (fast_original_path_) {
        phase_++;
        verify(phase_ % n_phase == Phase::ORIGIN);
#ifdef CURP_FULL_LOG_DEBUG
        Log_info("[CURP] coo_id=%d cmd<%d, %d> fastpath fail, OriginalProtocol", coo_id_, SimpleRWCommand::GetCmdID(sp_vpd_).first, SimpleRWCommand::GetCmdID(sp_vpd_).second);
#endif
        OriginalProtocol();
      } else {
#ifdef CURP_FULL_LOG_DEBUG
        Log_info("[CURP] coo_id=%d cmd<%d, %d> fastpath fail, QueryCoordinator", coo_id_, SimpleRWCommand::GetCmdID(sp_vpd_).first, SimpleRWCommand::GetCmdID(sp_vpd_).second);
#endif
        QueryCoordinator();
      }
      break;
    case Phase::QUERY:
      verify(phase_ % n_phase == Phase::ORIGIN);
      if (coordinator_success_) {
        coordinator_success_ = false;
#ifdef CURP_FULL_LOG_DEBUG
        ("[CURP] coo_id=%d cmd<%d, %d> QueryCoordinator success", coo_id_, SimpleRWCommand::GetCmdID(sp_vpd_).first, SimpleRWCommand::GetCmdID(sp_vpd_).second);
#endif
        committed_ = true;
        phase_++;
        verify(phase_ % n_phase == Phase::INIT_END);
        coordinatoraccept_count_++;
        cli2cli_[1].append(SimpleRWCommand::GetCurrentMsTime() - dispatch_time_);
        End();
      } else {
#ifdef CURP_FULL_LOG_DEBUG
        Log_info("[CURP] coo_id=%d cmd<%d, %d> QueryCoordinator fail, OriginalProtocol", coo_id_, SimpleRWCommand::GetCmdID(sp_vpd_).first, SimpleRWCommand::GetCmdID(sp_vpd_).second);
#endif
        OriginalProtocol();
      }
      break;
    case Phase::ORIGIN:
      verify(phase_ % n_phase == Phase::INIT_END);
#ifdef CURP_FULL_LOG_DEBUG
      Log_info("[CURP] coo_id=%d cmd<%d, %d> Original Protocol success", coo_id_, SimpleRWCommand::GetCmdID(sp_vpd_).first, SimpleRWCommand::GetCmdID(sp_vpd_).second);
#endif
      committed_ = true;
      original_protocol_count_++;
      if (fast_original_path_)
        cli2cli_[2].append(SimpleRWCommand::GetCurrentMsTime() - dispatch_time_);
      else
        cli2cli_[3].append(SimpleRWCommand::GetCurrentMsTime() - dispatch_time_);
      End();
      break;
    default:
      verify(0);
  }
}

void CoordinatorCurp::BroadcastDispatch() {
  auto txn = (TxData*) cmd_;
  auto n_pd = Config::GetConfig()->n_parallel_dispatch_;
  n_pd = 100;
  auto cmds_by_par = txn->GetReadyPiecesData(n_pd); // TODO setting n_pd larger than 1 will cause 2pl to wait forever
  cmds_by_par_ = cmds_by_par;
  curp_stored_cmd_ = true;
  Log_debug("Dispatch for tx_id: %" PRIx64, txn->root_id_);
  // [CURP] TODO: only support partition = 1 now
  verify(cmds_by_par.size() == 1);
  shared_ptr<CurpDispatchQuorumEvent> e;
  for (auto& pair: cmds_by_par) {
    const parid_t& par_id = pair.first;
    auto& cmds = pair.second;
    n_dispatch_ += cmds.size();
    auto sp_vec_piece = std::make_shared<vector<shared_ptr<TxPieceData>>>();
    for (auto c: cmds) {
      c->id_ = next_pie_id();
      dispatch_acks_[c->inn_id_] = false;
      sp_vec_piece->push_back(c);
    }
    verify(sp_vec_piece->size() == 1); // for Curp setting
    cmdid_t cmd_id = sp_vec_piece->at(0)->root_id_;
    verify(sp_vec_piece->size() > 0);
    verify(par_id == sp_vec_piece->at(0)->PartitionId());
    shared_ptr<VecPieceData> sp_vpd(new VecPieceData);
    sp_vpd->sp_vec_piece_data_ = sp_vec_piece;
    sp_vpd_ = sp_vpd;
    e = commo()->CurpBroadcastDispatch(sp_vpd);
  }
  e->Wait();
  // Log_info("[CURP] After Wait");
  if (e->FastYes()) {
    fast_path_success_ = true;
  } else if (e->FastNo() || e->timeouted_) {
    fast_path_success_ = false;
  } else {
    verify(0);
  }
  curp_coo_id_ = e->GetCooId();
  finish_countdown_ = e->GetFinishCountdown();
  key_hotness_ = e->GetKeyHotness();
  fast_original_path_ = finish_countdown_ > 0 || key_hotness_ > 1;
  // Log_info("[CURP] finish_countdown_ = %d key_hotness_ = %d", finish_countdown_, key_hotness_);
  GotoNextPhase();
}

void CoordinatorCurp::QueryCoordinator() {
  shared_ptr<CurpWaitCommitQuorumEvent> wait_quorum = commo_->CurpBroadcastWaitCommit(sp_vpd_, curp_coo_id_);
  wait_quorum->Wait();
  if (wait_quorum->committed_) {
    coordinator_success_ = true;
  } else {
    coordinator_success_ = false;
  }
  value_t commit_result_ = wait_quorum->commit_result_;
  GotoNextPhase();
}

void CoordinatorCurp::OriginalProtocol() {
  CurpDispatchAsync();
  // GotoNextPhase();
}

} // namespace janus
