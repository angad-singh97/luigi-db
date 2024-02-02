//
// Created by shuai on 11/25/15.
//
#pragma once

#include "../scheduler.h"

namespace janus {

class TxData;
class TpcPrepareCommand;
class TpcCommitCommand;
class SimpleCommand;
class SchedulerClassic: public TxLogServer {
 using TxLogServer::TxLogServer;
 public:

  bool slow_ = false;

  Distribution cli2tx, tx2tx;

  SchedulerClassic() {}

  ~SchedulerClassic() {
    if (cli2tx.count() != 0 || tx2tx.count() != 0)
      Log_info("[CURP] TxSvr loc_id_=%d site_id_=%d \
                cli2tx 50% = %.2f ms; cli2tx 90% = %.2f ms; cli2tx 99% = %.2f ms; \
                tx2tx 50% = %.2f ms; tx2tx 90% = %.2f ms; tx2tx 99% = %.2f ms;",
                loc_id_, site_id_,
                cli2tx.pct50(), cli2tx.pct90(), cli2tx.pct99(),
                tx2tx.pct50(), tx2tx.pct90(), tx2tx.pct99());
    else
      Log_info("[CURP] TxSvr loc_id_=%d site_id_=%d No Latency Measured", loc_id_, site_id_);
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

  virtual void OnRuleSpeculativeExecute(shared_ptr<Marshallable> cmd,
                                        bool_t* accepted,
                                        int32_t* result) override;

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
