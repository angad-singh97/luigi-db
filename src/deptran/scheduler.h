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

class CurpDispatchQuorumEvent;

class CurpPlusData : public enable_shared_from_this<CurpPlusData>{
 private:
  key_t key_;
  ver_t ver_;
  shared_ptr<Marshallable> cmd_{nullptr};
 public:
  TxLogServer* svr_{nullptr};
  enum CurpPlusStatus {
    INIT = 0,
    PREACCEPT = 1,
    // PREPARED = 2,
    ACCEPTED = 3,
    COMMITTED = 4,
    EXECUTED = 5,
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
  pair<key_t, ver_t> GetPos() {
    return make_pair(key_, ver_);
  }
  void PrepareInstance();
};

class CurpPlusDataCol {
 private:
  size_t latest_executed_ver_ = 0;
  TxLogServer* svr_{nullptr};
  key_t key_;
 public:
  bool in_applying_logs_{false};
  // use this log from position 1, position 0 is intentionally left blank to match the usage of "slot" & "slotid_t"
  map<slotid_t, shared_ptr<CurpPlusData>> logs_{};
  CurpPlusDataCol(TxLogServer* svr, key_t key): svr_(svr), key_(key){
#ifdef CURP_FULL_LOG_DEBUG
    Log_info("[CURP] Create CurpPlusDataCol for key=%d", key);
#endif
    logs_[0] = nullptr;
  }
  int32_t LatestExecutedVer() {
    return latest_executed_ver_;
  }
  int32_t NextVersion() {
    return latest_executed_ver_ + 1;
  }
  shared_ptr<CurpPlusData> RecentExecutedInstance() {
    return Get(latest_executed_ver_);
  }
  shared_ptr<CurpPlusData> NextInstance() {
    return GetOrCreate(latest_executed_ver_ + 1);
  }
  shared_ptr<CurpPlusData> Get(ver_t ver) {
    return logs_[ver];
  }
  shared_ptr<CurpPlusData> GetOrCreate(ver_t ver);
  void Execute(ver_t ver);
  void Print();
  // latest_executed_ver_ to ver may exist hole
  void CheckHoles(ver_t ver);
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
    if (data.size() == 0)
      return -1;
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

class CurpInstanceCommitTimeoutPool {
 public:
  set<pair<key_t, ver_t>> in_pool_;
  set<pair<double, shared_ptr<CurpPlusData>>> pool_;
  CurpInstanceCommitTimeoutPool() {
    Coroutine::CreateRun([this]() { 
      TimeoutLoop();
    });
  }
  void TimeoutLoop();
  void AddTimeoutInstance(shared_ptr<CurpPlusData> instance, bool repeat_insert = false);
};

class CurpCoordinatorCommitFinishTimeoutPool {
  public:
    TxLogServer *sch_ = nullptr;
    // <key> exist in this pool iff there exist a Finish symbol related to <key> in process (either in wait_for_commit_events_pool_)
    set<key_t> in_pool_;
    // This pool manages all QuorumEvents that commits Finish along with original protocol, key is original protocol cmd_id
    unordered_map<int64_t, pair<key_t, shared_ptr<CurpDispatchQuorumEvent>>> wait_for_commit_events_pool_{};
    CurpCoordinatorCommitFinishTimeoutPool(TxLogServer *sch): sch_(sch) {
      Coroutine::CreateRun([this]() { 
        TimeoutLoop();
      });
    };
    void TimeoutLoop();
    void DealWith(int64_t cmd_id);
};

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
  std::recursive_mutex curp_mtx_{};

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

  int curp_fastpath_timeout_count_ = 0;
  int curp_wait_commit_timeout_count_ = 0;
  int curp_instance_commit_timeout_trigger_prepare_count_ = 0;
  int finish_countdown_count_ = 0;

  int curp_unique_original_cmd_id_ = 0;

  map<key_t, int> curp_key_hotness_;

  // CURP Timestamp debug
  Distribution cli2svr_dispatch, cli2svr_commit;
#ifdef LATENCY_DEBUG
  Distribution cli2preskip_begin_, cli2preskip_end_, cli2skip_begin_, cli2skip_end_;
  Distribution cli2leader_recv_, cli2leader_send_, cli2follower_recv_, cli2follower_send_, cli2commit_send_, cli2oncommit_;
  Distribution preskip_attemp_;
#endif

  // CURP commit finish in advance
  map<key_t, bool> curp_in_commit_finish_;

  // CURP coordinator reply/notify the client: cmd_id index
  map<pair<int32_t, int32_t>, shared_ptr<CommitNotification>> executed_results_;
  vector<shared_ptr<CommitNotification>> commit_timeout_list_;
  int commit_timeout_solved_count_;

  map<key_t, int> finish_countdown_;

  // application k-v table for rw workload
  map<key_t, value_t> kv_table_;

  // [CURP] TODO: discard assigned_ since it is used to debug a old bug which have been solved
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
  // bool curp_in_applying_logs_{false};

  CurpInstanceCommitTimeoutPool curp_instance_commit_timeout_pool_;
  CurpCoordinatorCommitFinishTimeoutPool curp_coordinator_commit_finish_timeout_pool_;

  map<pair<key_t, ver_t>, shared_ptr<ResponseData>> curp_response_storage_;

  void OnCurpDispatch(const int32_t& client_id,
                      const int32_t& cmd_id_in_client,
                      const shared_ptr<Marshallable>& cmd,
                      bool_t* accepted,
                      ver_t* ver,
                      value_t* result,
                      int32_t* finish_countdown,
                      int32_t* key_hotness,
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

  void CurpPreSkipFastpath(shared_ptr<Marshallable> &cmd);

  void CurpSkipFastpath(int cmd_id, shared_ptr<Marshallable> &cmd);

  uint64_t CurpAttemptCommitFinish(shared_ptr<Marshallable> &cmd);

  void CurpAttemptCommitFinishReply(pair<int32_t, int32_t> cmd_id,
                                    bool_t &finish_accept,
                                    uint64_t &finish_ver);

  void OnCurpAttemptCommitFinish(shared_ptr<Marshallable> &cmd,
                                const uint64_t& commit_finish,
                                bool_t* finish_accept,
                                uint64_t* finish_ver);

  shared_ptr<CurpPlusData> GetCurpLog(key_t key, ver_t ver);

  shared_ptr<CurpPlusData> GetOrCreateCurpLog(key_t key, ver_t ver);

  UniqueCmdID GetUniqueCmdID(shared_ptr<Marshallable> cmd);

  value_t DBGet(const shared_ptr<Marshallable>& cmd);

  value_t DBPut(const shared_ptr<Marshallable>& cmd);
};

} // namespace janus
