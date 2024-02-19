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
  if (Config::GetConfig()->replica_proto_ == MODE_FPGA_RAFT) {
    margin_success_rate_ = 0.724;
  } else if (Config::GetConfig()->replica_proto_ == MODE_COPILOT) {
    margin_success_rate_ = 0.713;
  } else if (Config::GetConfig()->replica_proto_ == MODE_MENCIUS) {
    margin_success_rate_ = 0.900;
  } else {
    verify(0);
  }
  // Log_info("[CURP] CoordinatorRule created for coo_id=%d thread_id=%d", coo_id, thread_id);
}

void CoordinatorRule::GotoNextPhase() {
  int n_phase = 3;
  int current_phase = phase_ % n_phase;
  switch (phase_++ % n_phase) {
    case Phase::INIT_END:
      dispatch_time_ = SimpleRWCommand::GetCurrentMsTime();
      dispatch_duration_3_times_ = (dispatch_time_ - created_time_) * 3;
      verify(phase_ % n_phase == Phase::DISPATCHED);
      fast_path_success_ = false;
      dispatch_ack_ = false;
      // Log_info("CoordinatorRule coo_id=%d thread_id=%d cmd_ver_=%d current_phase=%d [before dispatch]", coo_id_, thread_id_, cmd_ver_, current_phase);
      DispatchAsync();
      // Log_info("CoordinatorRule coo_id=%d thread_id=%d cmd_ver_=%d current_phase=%d [before BroadcastRuleSpeculativeExecute]", coo_id_, thread_id_, cmd_ver_, current_phase);
      // BroadcastRuleSpeculativeExecute(cmd_ver_);
      if (0 <= Config::GetConfig()->curp_or_rule_fastpath_rate_ && Config::GetConfig()->curp_or_rule_fastpath_rate_ <= 100) {
        // fixed percentage
        go_to_fastpath_ = RandomGenerator::rand(0, 99) < Config::GetConfig()->curp_or_rule_fastpath_rate_;
      } else if (Config::GetConfig()->curp_or_rule_fastpath_rate_ == 101) {
        if (dispatch_duration_3_times_ < Config::GetConfig()->duration_ * 1000) {
          go_to_fastpath_ = true;
        } else {
          // Log_info("Fastpath success: %.6f margin_success_rate: %.6f", recent_fastpath_success_.ave(), margin_success_rate_);
          go_to_fastpath_ = recent_fastpath_success_.ave() > margin_success_rate_;
        }
      } else {
        verify(0);
      }
      if (go_to_fastpath_) {
        if (dispatch_duration_3_times_ > Config::GetConfig()->duration_ * 1000 && dispatch_duration_3_times_ < Config::GetConfig()->duration_ * 2 * 1000) {
          fastpath_attempted_count_++;
        }
        BroadcastRuleSpeculativeExecute(cmd_ver_);
      } else {
        // Do nothing
      }
      break;
    case Phase::DISPATCHED:
      if (go_to_fastpath_) {
        if (fast_path_success_)
          recent_fastpath_success_.append(1);
        else
          recent_fastpath_success_.append(0);
      }
      if (fast_path_success_ || dispatch_ack_) {
        committed_ = true;
        // verify(phase_ % n_phase == Phase::WAITING_ORIGIN);
        phase_++;
        verify(phase_ % n_phase == Phase::INIT_END);
        // Log_info("CoordinatorRule coo_id=%d thread_id=%d cmd_ver_=%d current_phase=%d [before dispatch end] fast_path_success_=%d dispatch_ack_=%d", coo_id_, thread_id_, cmd_ver_, current_phase, fast_path_success_, dispatch_ack_);
        if (dispatch_duration_3_times_ > Config::GetConfig()->duration_ * 1000 && dispatch_duration_3_times_ < Config::GetConfig()->duration_ * 2 * 1000) {
          // verify(!(fast_path_success_ && dispatch_ack_));
          if (fast_path_success_) {
            fastpath_successed_count_++;
            cli2cli_[0].append(SimpleRWCommand::GetCurrentMsTime() - dispatch_time_);
          }
          else
            cli2cli_[4].append(SimpleRWCommand::GetCurrentMsTime() - dispatch_time_);
        }
        cmd_ver_++;
        End();
      } else {
        verify(phase_ % n_phase == Phase::WAITING_ORIGIN);
        // Log_info("CoordinatorRule coo_id=%d thread_id=%d cmd_ver_=%d current_phase=%d [before into WAITING_ORIGIN] fast_path_success_=%d dispatch_ack_=%d", coo_id_, thread_id_, cmd_ver_, current_phase, fast_path_success_, dispatch_ack_);
      }
      break;
    case Phase::WAITING_ORIGIN:
      committed_ = true;
      verify(phase_ % n_phase == Phase::INIT_END);
      // Log_info("CoordinatorRule coo_id=%d thread_id=%d cmd_ver_=%d current_phase=%d [before WAITING_ORIGIN end]", coo_id_, thread_id_, cmd_ver_, current_phase);
      if (dispatch_duration_3_times_ > Config::GetConfig()->duration_ * 1000 && dispatch_duration_3_times_ < Config::GetConfig()->duration_ * 2 * 1000) 
          cli2cli_[4].append(SimpleRWCommand::GetCurrentMsTime() - dispatch_time_);
      cmd_ver_++;
      End();
      break;
    default:
      verify(0);
  }
}

void CoordinatorRule::BroadcastRuleSpeculativeExecute(int cmd_ver) {
  auto txn = (TxData*) cmd_;
  auto n_pd = Config::GetConfig()->n_parallel_dispatch_;
  n_pd = 100;
  // auto cmds_by_par = txn->GetReadyPiecesData(n_pd); // TODO setting n_pd larger than 1 will cause 2pl to wait forever
  auto cmds_by_par = cmds_by_par_;
  // curp_stored_cmd_ = true;
  Log_debug("Dispatch for tx_id: %" PRIx64, txn->root_id_);
  // [CURP] TODO: only support partition = 1 now
  verify(cmds_by_par.size() == 1);
  shared_ptr<RuleSpeculativeExecuteQuorumEvent> e;
  for (auto& pair: cmds_by_par) {
    const parid_t& par_id = pair.first;
    auto& cmds = pair.second;
    // n_dispatch_ += cmds.size();
    auto sp_vec_piece = sp_vec_piece_by_par_[par_id];
    // for (auto c: cmds) {
    //   c->id_ = next_pie_id();
    //   dispatch_acks_[c->inn_id_] = false;
    //   sp_vec_piece->push_back(c);
    // }
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
  // if (cmd_ver != cmd_ver_) return;
  // Log_info("[CURP] After Wait");
  if (e->Yes()) {
    fast_path_success_ = true;
  } else if (e->No() || e->timeouted_) {
    fast_path_success_ = false;
  } else {
    verify(0);
  }
  result_ = e->GetResult();
  // fast_path_success_ = false;
  if (cmd_ver != cmd_ver_) return;
  GotoNextPhase();
}

void CoordinatorRule::DispatchAsync() {
  Log_debug("commo Broadcast to the server on client worker");
  std::lock_guard<std::recursive_mutex> lock(mtx_);
  auto txn = (TxData*) cmd_;

  int cnt = 0;
  auto n_pd = Config::GetConfig()->n_parallel_dispatch_;
  n_pd = 100;
  ReadyPiecesData cmds_by_par;
  cmds_by_par = txn->GetReadyPiecesData(n_pd); // TODO setting n_pd larger than 1 will cause 2pl to wait forever
  cmds_by_par_ = cmds_by_par;
  Log_debug("Dispatch for tx_id: %" PRIx64, txn->root_id_);
  sp_vec_piece_by_par_.clear();
  for (auto& pair: cmds_by_par) {
    const parid_t& par_id = pair.first;
    auto& cmds = pair.second;
    n_dispatch_ += cmds.size();
    cnt += cmds.size();
    auto sp_vec_piece = std::make_shared<vector<shared_ptr<TxPieceData>>>();
    for (auto c: cmds) {
      c->id_ = next_pie_id();
      dispatch_acks_[c->inn_id_] = false;
      sp_vec_piece->push_back(c);
      frequency_.append(SimpleRWCommand::GetKey(c));
    }
    sp_vec_piece_by_par_[par_id] = sp_vec_piece;
    commo()->BroadcastDispatch(sp_vec_piece,
                               this,
                               std::bind(&CoordinatorClassic::DispatchAck,
                                         this,
                                         cmd_ver_,
                                         phase_,
                                         std::placeholders::_1,
                                         std::placeholders::_2));
  }
}

} // namespace janus
