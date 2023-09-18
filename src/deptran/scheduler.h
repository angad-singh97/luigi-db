#pragma once
#include "__dep__.h"
#include "constants.h"
#include "command.h"
#include "epochs.h"
#include "kvdb.h"
#include "procedure.h"
#include "tx.h"
#include "rcc/tx.h"
#include "classic/tpc_command.h"
#include "RW_command.h"

namespace janus {

/*****************************CURP begin************************************/
struct UniqueCmdID {
  int32_t client_id_;
  int32_t cmd_id_;
};

shared_ptr<Marshallable> MakeFinishCmd(parid_t par_id, int cmd_id, key_t key, value_t value);
shared_ptr<Marshallable> MakeNoOpCmd(parid_t par_id);

class CurpPlusData {
 private:
  TxLogServer* svr_{nullptr};
  key_t key_;
  ver_t ver_;
  shared_ptr<Marshallable> cmd_{nullptr};
 public:
  enum CurpPlusStatus {
    INIT = 0,
    PREACCEPT = 1,
    // PREPARED = 2,
    ACCEPTED = 3,
    COMMITTED = 4,
  };
  CurpPlusStatus status_;
  ballot_t max_seen_ballot_ = -1;
  ballot_t last_accepted_ballot_ = -1;

  // unzip from cmd
  int32_t type_;
  // key_t key_;
  value_t value_;

  CurpPlusData(TxLogServer* svr, key_t key, ver_t ver, CurpPlusStatus status, const shared_ptr<Marshallable> &cmd, ballot_t ballot);

  void UpdateCmd(const shared_ptr<Marshallable> &cmd);
  shared_ptr<Marshallable> GetCmd();
  void InstanceCommitTimeout();
};

class CurpPlusDataCol {
 private:
  size_t recent_executed_ = 0;
  TxLogServer* svr_{nullptr};
  key_t key_;
 public:
  bool in_applying_logs_{false};
  // use this log from position 1, position 0 is intentionally left blank to match the usage of "slot" & "slotid_t"
  map<slotid_t, shared_ptr<CurpPlusData>> logs_{};
  CurpPlusDataCol(TxLogServer* svr, key_t key): svr_(svr), key_(key){
    Log_info("[CURP] Create CurpPlusDataCol for key=%d", key);
    logs_[0] = nullptr;
  }
  int32_t RecentVersion() {
    return recent_executed_;
  }
  int32_t NextVersion() {
    return recent_executed_ + 1;
  }
  shared_ptr<CurpPlusData> RecentExecuted() {
    return Get(recent_executed_);
  }
  shared_ptr<CurpPlusData> NextInstance() {
    return GetOrCreate(recent_executed_ + 1);
  }
  shared_ptr<CurpPlusData> Get(ver_t ver) {
    return logs_[ver];
  }
  shared_ptr<CurpPlusData> GetOrCreate(ver_t ver);
  void Executed(ver_t ver) {
    verify(recent_executed_ + 1 == ver);
    recent_executed_ = ver;
  }
  void print();
};

class Distribution {
 public:
  vector<double> data;
  void append(double x) {
    data.push_back(x);
  }
  void merge(Distribution &o) {
    for (int i = 0; i < o.count(); i++)
      data.push_back(o.data[i]);
  }
  size_t count() {
    return data.size();
  }
  double pct(double pct) {
    sort(data.begin(), data.end());
    return data[floor(data.size() * pct)];
  }
  double pct50() {
    return pct(0.5);
  }
  double pct90() {
    return pct(0.9);
  }
  double pct99() {
    return pct(0.99);
  }
};

struct ResponseData {
  pair<ver_t, ver_t> pos_of_this_pack;
  map<pair<int, int>, vector<shared_ptr<Marshallable> > >responses_;
  shared_ptr<Marshallable> max_cmd_{nullptr};
  int received_count_ = 0, accept_count_ = 0, max_accept_count_ = 0;
  double first_seen_time_ = 0;
  bool done_{false};
  pair<int, int> append_response(const shared_ptr<Marshallable>& cmd) {
    VecPieceData *vecPiece;
    if (cmd->kind_ == MarshallDeputy::CMD_TPC_COMMIT) { // original through tx svr
      shared_ptr<TpcCommitCommand> tpc_cmd = dynamic_pointer_cast<TpcCommitCommand>(cmd);
      vecPiece = (VecPieceData*)(tpc_cmd->cmd_.get());
    } else if (cmd->kind_ == MarshallDeputy::CMD_VEC_PIECE) { // curp broadcast
      vecPiece = dynamic_pointer_cast<VecPieceData>(cmd).get();
    } else {
      verify(0);
    }
    shared_ptr<CmdData> md = vecPiece->sp_vec_piece_data_->at(0);
    pair<int, int> cmd_id = {md->client_id_, md->cmd_id_in_client_};
    responses_[cmd_id].push_back(cmd);
    accept_count_++;
    if (responses_[cmd_id].size() > max_accept_count_) {
      max_accept_count_ = responses_[cmd_id].size();
      max_cmd_ = cmd;
    }
    return {accept_count_, max_accept_count_};
  }
  shared_ptr<Marshallable> GetMaxCmd() {
    return max_cmd_;
  }
};

struct CommitNotification {
  // client side
  bool client_stored_ = false;
  bool_t* committed_;
  value_t* commit_result_;
  function<void()> commit_callback_;
  // coordinator side
  bool coordinator_stored_ = false;
  value_t coordinator_commit_result_;
  bool coordinator_replied_ = false;
  // timestamp (ms)
  double receive_time_ = -1;
};

  /*****************************CURP end************************************/

class TxnRegistry;
class Executor;
class Coordinator;
class Frame;
class Communicator;
class TxLogServer {
 public:
  void *svr_workers_g{nullptr};

  locid_t loc_id_ = -1;
  siteid_t site_id_ = -1;
  unordered_map<txid_t, shared_ptr<Tx>> dtxns_{};
  unordered_map<txid_t, mdb::Txn *> mdb_txns_{};
  unordered_map<txid_t, Executor *> executors_{};

  function<void(Marshallable &)> app_next_{};
  function<shared_ptr<vector<MultiValue>>(Marshallable&)> key_deps_{};

  shared_ptr<mdb::TxnMgr> mdb_txn_mgr_{};
  int mode_;
  Recorder *recorder_ = nullptr;
  Frame *frame_ = nullptr;
  Frame *rep_frame_ = nullptr;
  // Frame *curp_rep_frame_ = nullptr;
  TxLogServer *tx_sched_ = nullptr;
  // rep_sched_ and curp_rep_sched_ is originnally used by tx_schduler, but afterwards also used be link rep_sched_ and curp_rep_sched_
  TxLogServer *rep_sched_ = nullptr;
  // TxLogServer *curp_rep_sched_ = nullptr;
  Communicator *commo_{nullptr};
  //  Coordinator* rep_coord_ = nullptr;
  shared_ptr<TxnRegistry> txn_reg_{nullptr};
  parid_t partition_id_{};
  std::recursive_mutex mtx_{};
  // std::mutex mtx2_{};

  bool epoch_enabled_{false};
  EpochMgr epoch_mgr_{};
  std::time_t last_upgrade_time_{0};
  map<parid_t, map<siteid_t, epoch_t>> epoch_replies_{};
  bool in_upgrade_epoch_{false};
  const int EPOCH_DURATION = 5;

  // CURP countings
  int curp_fast_path_success_count_ = 0;
  int curp_coordinator_accept_count_ = 0;
  int original_protocol_submit_count_ = 0;

  // CURP Timestamp debug
  Distribution cli2svr_dispatch, cli2svr_commit;

  // CURP coordinator reply/notify the client: cmd_id index
  map<pair<int32_t, int32_t>, shared_ptr<CommitNotification>> commit_results_;
  vector<shared_ptr<CommitNotification>> commit_timeout_list_;
  int commit_timeout_solved_count_;

  map<key_t, int> finish_countdown_;

  // application k-v table for rw workload
  map<key_t, value_t> kv_table_;

  map<pair<int, int>, bool> assigned_;
#ifdef CHECK_ISO
  typedef map<Row*, map<colid_t, int>> deltas_t;
  deltas_t deltas_{};

  void MergeDeltas(deltas_t deltas) {
    verify(deltas.size() > 0);
    for (auto& pair1: deltas) {
      Row* r = pair1.first;
      for (auto& pair2: pair1.second) {
        colid_t c = pair2.first;
        int delta = pair2.second;
        deltas_[r][c] += delta;
        int v = r->get_column(c).get_i32();
        int x = deltas_[r][c];
      }
    }
    deltas.clear();
  }

  void CheckDeltas() {
    for (auto& pair1: deltas_) {
      Row* r = pair1.first;
      for (auto& pair2: pair1.second) {
        colid_t c = pair2.first;
        int delta = pair2.second;
        int v = r->get_column(c).get_i32();
        verify(delta == v);
      }
    }
  }
#endif

  Communicator *commo() {
    verify(commo_ != nullptr);
    return commo_;
  }

  TxLogServer();
  TxLogServer(int mode);
  virtual ~TxLogServer();


  virtual void SetPartitionId(parid_t par_id) {
    partition_id_ = par_id;
  }

  // runs in a coroutine.

  virtual bool HandleConflicts(Tx &dtxn,
                               innid_t inn_id,
                               vector<string> &conflicts) {
    return false;
  };
  virtual bool HandleConflicts(Tx &dtxn,
                               innid_t inn_id,
                               vector<conf_id_t> &conflicts) {
    Log_fatal("unimplemnted feature: handle conflicts!");
    return false;
  };
  virtual void Execute(Tx &txn_box,
                       innid_t inn_id);

  Coordinator *CreateRepCoord(const i64& dep_id=0);
  // Coordinator *CreateCurpRepCoord(const i64& dep_id=0);
  virtual shared_ptr<Tx> GetTx(txnid_t tx_id);
  virtual shared_ptr<Tx> CreateTx(txnid_t tx_id,
                                  bool ro = false);
  virtual shared_ptr<Tx> CreateTx(epoch_t epoch,
                                  txnid_t txn_id,
                                  bool read_only = false);
  virtual shared_ptr<Tx> GetOrCreateTx(txnid_t tid, bool ro = false);
  void DestroyTx(i64 tid);

  virtual void DestroyExecutor(txnid_t txn_id);

  inline int get_mode() { return mode_; }

  // Below are function calls that go deeper into the mdb.
  // They are merged from the called TxnRunner.

  inline mdb::Table
  *get_table(const string &name) {
    return mdb_txn_mgr_->get_table(name);
  }

  virtual mdb::Txn *GetMTxn(const i64 tid);
  virtual mdb::Txn *GetOrCreateMTxn(const i64 tid);
  virtual mdb::Txn *RemoveMTxn(const i64 tid);

  void get_prepare_log(i64 txn_id,
                       const std::vector<i32> &sids,
                       std::string *str
  );

  // TODO: (Shuai: I am not sure this is supposed to be here.)
  // I think it used to initialized the database?
  // So it should be somewhere else?
  void reg_table(const string &name,
                 mdb::Table *tbl
  );

  virtual bool Dispatch(cmdid_t cmd_id,
                        shared_ptr<Marshallable> cmd,
                        TxnOutput& ret_output) {
    verify(0);
    return false;
  }

  void RegLearnerAction(function<void(Marshallable &)> learner_action) {
    app_next_ = learner_action;
  }

  /**
   * Check if the command is already committed
   * @param commit_cmd command to be checked
   * @return true if it's already committed, false otherwise
   */
  virtual bool CheckCommitted(Marshallable& commit_cmd) { verify(0); }

  virtual void Next(Marshallable& cmd) { verify(0); };

	virtual void Setup() { verify(0); } ;
  virtual bool IsLeader() { verify(0); } ;
  virtual bool IsFPGALeader() { verify(0); } ;
	
	virtual bool RequestVote() { verify(0); return false;};
  virtual void Pause() { verify(0); } ;
  virtual void Resume() { verify(0); } ;

  // epoch related functions
  void TriggerUpgradeEpoch();
  void UpgradeEpochAck(parid_t par_id, siteid_t site_id, int res);
  virtual int32_t OnUpgradeEpoch(uint32_t old_epoch);

  // below are about CURP

  map<key_t, shared_ptr<CurpPlusDataCol>> curp_log_cols_{};
  // [CURP] TODO: this need to be used
  // int n_prepare_ = 0;
  // int n_accept_ = 0;
  // int n_commit_ = 0;
  int n_fast_path_attempted_ = 0;
  int n_fast_path_failed_ = 0;
  // bool curp_in_applying_logs_{false};

  map<pair<key_t, ver_t>, shared_ptr<ResponseData>> curp_response_storage_;

  void OnCurpDispatch(const int32_t& client_id,
                      const int32_t& cmd_id_in_client,
                      const shared_ptr<Marshallable>& cmd,
                      bool_t* accepted,
                      ver_t* ver,
                      value_t* result,
                      siteid_t* coo_id,
                      const function<void()> &cb);

  void OnCurpWaitCommit(const int32_t& client_id,
                    const int32_t& cmd_id_in_client,
                    bool_t* committed,
                    value_t* commit_result,
                    const function<void()> &cb);

  void OnCurpForward(const bool_t& accepted,
                      const ver_t& ver,
                      const shared_ptr<Marshallable>& cmd);

  void CurpPrepare(key_t key,
                    ver_t ver,
                    ballot_t ballot);

  void CurpAccept(ver_t ver,
                  ballot_t ballot,
                  const shared_ptr<Marshallable>& cmd);

  void CurpCommit(ver_t ver,
                  shared_ptr<Marshallable> cmd);

  void OnCurpPrepare(const key_t& k,
                      const ver_t& ver,
                      const ballot_t& ballot,
                      bool_t* accepted,
                      int* status,
                      ballot_t* last_accepted_ballot,
                      MarshallDeputy* md_cmd,
                      const function<void()> &cb);

  void OnCurpAccept(const ver_t& ver,
                    const ballot_t& ballot,
                    const shared_ptr<Marshallable>& cmd,
                    bool_t* accepted,
                    ballot_t* seen_ballot,
                    const function<void()> &cb);

  void OnCurpCommit(const ver_t& ver,
                    const shared_ptr<Marshallable>& cmd);

  void OnOriginalSubmit(shared_ptr<Marshallable> &cmd,
                        const rrr::i64& dep_id,
                        bool_t* slow,
                        const function<void()> &cb);

  void CurpSkipFastpath(int cmd_id, shared_ptr<Marshallable> &cmd);

  shared_ptr<CurpPlusData> GetCurpLog(key_t key, ver_t ver);

  shared_ptr<CurpPlusData> GetOrCreateCurpLog(key_t key, ver_t ver);

  UniqueCmdID GetUniqueCmdID(shared_ptr<Marshallable> cmd);
 private:
  value_t DBGet(const shared_ptr<Marshallable>& cmd);
  value_t DBPut(const shared_ptr<Marshallable>& cmd);
};

} // namespace janus
