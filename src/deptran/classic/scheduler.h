//
// Created by shuai on 11/25/15.
//
#pragma once

#include "../scheduler.h"
#include "../position.h"

namespace janus {

class TxData;
class TpcPrepareCommand;
class TpcCommitCommand;
class SimpleCommand;
class SchedulerClassic: public TxLogServer {
 using TxLogServer::TxLogServer;
 public:

  bool slow_ = false;

  double latency_sum = 0;
  int latency_count = 0;
  double latency_max = 0;
  double latency_min = 1e9;

  SchedulerClassic() {}

  ~SchedulerClassic() {
    if (latency_count != 0)
      Log_info("[CURP] Ave Latency %.2f ms; Min Latency %.2f ms; Max Latency %.2f ms", latency_sum/latency_count, latency_min, latency_max);
    else
      Log_info("[CURP] No Latency Measured");
  }

  void MergeCommands(vector<shared_ptr<TxPieceData>>&,
                     shared_ptr<Marshallable>);

  bool ExecutePiece(Tx& tx, TxPieceData& piece_data, TxnOutput& ret_output);

  virtual bool DispatchPiece(Tx& tx,
                             TxPieceData& cmd,
                             TxnOutput& ret_output);

  virtual bool Dispatch(cmdid_t cmd_id,
												struct DepId dep_id,
                        shared_ptr<Marshallable> cmd,
                        TxnOutput& ret_output);

  /**
   * For interactive pre-processing.
   * @param tx_box
   * @param row
   * @param col_id
   * @param write
   * @return
   */
  virtual bool Guard(Tx &tx_box, Row *row, int col_id, bool write=true) = 0;

  // PrepareRequest
  virtual bool OnPrepare(txnid_t tx_id,
                         const std::vector<i32> &sids,
                         struct DepId dep_id,
												 bool& null_cmd);

  virtual bool DoPrepare(txnid_t tx_id) {
    verify(0);
    return false;
  };

  virtual int OnCommit(cmdid_t cmd_id,
                       struct DepId dep_id,
                       int commit_or_abort);

  virtual int OnEarlyAbort(txid_t tx_id);

  virtual void DoCommit(Tx& tx_box);

  virtual void DoAbort(Tx& tx_box);

  virtual void Next(Marshallable&) override;

  virtual bool IsLeader() override { return rep_sched_->IsLeader(); }

  virtual bool IsFPGALeader() override { return rep_sched_->IsFPGALeader(); }
	
	virtual bool RequestVote() override {
		return rep_sched_->RequestVote();
	}

  int PrepareReplicated(TpcPrepareCommand& prepare_cmd);
  int CommitReplicated(TpcCommitCommand& commit_cmd);

  bool CheckCommitted(Marshallable& commit_cmd) override;

private:

  void FastPathManagerSubmit(shared_ptr<Coordinator> coo, shared_ptr<Marshallable> sp_m);

};

} // namespace janus
