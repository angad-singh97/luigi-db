#include "coordinator.h"
#include "frame.h"
#include "benchmark_control_rpc.h"
#include "../RW_command.h"
#include "../../rrr/misc/rand.hpp"
#include "commo.h"

namespace janus {

// This Coordinator should be on Client Side

CoordinatorRule::CoordinatorRule(uint32_t coo_id,
                                       int32_t benchmark,
                                       ClientControlServiceImpl *ccsi,
                                       uint32_t thread_id)
  : CoordinatorClassic(coo_id, benchmark, ccsi, thread_id) {
  // Log_info("[CURP] CoordinatorRule created for coo_id=%d thread_id=%d", coo_id, thread_id);
}

void CoordinatorRule::GotoNextPhase() {
  int n_phase = 4;
  int current_phase = phase_ % n_phase;
  switch (phase_++ % n_phase) {
    case Phase::INIT_END:
      // dispatch_time_ = SimpleRWCommand::GetCurrentMsTime();
      // dispatch_duration_3_times_ = (dispatch_time_ - created_time_) * 3;

      cmd_term_++;
      phase_++;
      verify(phase_ % n_phase == Phase::DISPATCHED);
      DispatchAsync();
      BroadcastRuleSpeculativeExecute();
      break;
    case Phase::DISPATCHED:
      if (fast_path_success_) {
        phase_ += 2;
        verify(phase_ % n_phase == Phase::INIT_END);
        // if (dispatch_duration_3_times_ > Config::GetConfig()->duration_ * 1000 && dispatch_duration_3_times_ < Config::GetConfig()->duration_ * 2 * 1000)
        //   cli2cli_[0].append(SimpleRWCommand::GetCurrentMsTime() - dispatch_time_);
        End();
      } else {
        phase_++;
        verify(phase_ % n_phase == Phase::WAITING_ORIGIN);
      }
    case Phase::WAITING_ORIGIN:
      phase_ ++;
      verify(phase_ % n_phase == Phase::INIT_END);
      End();
    default:
      verify(0);
  }
}

void CoordinatorRule::BroadcastRuleSpeculativeExecute() {
  auto txn = (TxData*) cmd_;
  auto n_pd = Config::GetConfig()->n_parallel_dispatch_;
  n_pd = 100;
  auto cmds_by_par = txn->GetReadyPiecesData(n_pd); // TODO setting n_pd larger than 1 will cause 2pl to wait forever
  cmds_by_par_ = cmds_by_par;
  // curp_stored_cmd_ = true;
  Log_debug("Dispatch for tx_id: %" PRIx64, txn->root_id_);
  // [CURP] TODO: only support partition = 1 now
  verify(cmds_by_par.size() == 1);
  shared_ptr<RuleSpeculativeExecuteQuorumEvent> e;
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
    e = ((CommunicatorRule *)commo())->BroadcastRuleSpeculativeExecute(sp_vec_piece);
  }
  e->Wait();
  // Log_info("[CURP] After Wait");
  if (e->Yes()) {
    fast_path_success_ = true;
  } else if (e->No() || e->timeouted_) {
    fast_path_success_ = false;
  } else {
    verify(0);
  }
  result_ = e->GetResult();
  GotoNextPhase();
}

} // namespace janus
