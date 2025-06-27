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
#include "config.h"

namespace janus {

/*****************************CURP begin************************************/
struct UniqueCmdID {
  int32_t client_id_;
  int32_t cmd_id_;
};

// [CURP] TODO: discard cmd_id
shared_ptr<Marshallable> MakeFinishCmd(parid_t par_id, int cmd_id, key_t key, value_t value);
shared_ptr<Marshallable> MakeNoOpCmd(parid_t par_id, key_t key = -1);

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
  unordered_map<slotid_t, shared_ptr<CurpPlusData>> logs_{};
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
  double created_time_ = SimpleRWCommand::GetCurrentMsTime();
  double recent_100_sum_ = 0;
  // bool pct_lock = false;
 public:
  vector<double> data_;
  void append(double x) {
    // if (pct_lock) return;
    data_.push_back(x);
    recent_100_sum_ += x;
    if (data_.size() > 100)
      recent_100_sum_ -= data_[data_.size() - 101];
  }
  // only append if append_time is in mid 1/3 time (10~20s if duration is 30s)
  void mid_time_append(double x, double append_time) {
    // if (pct_lock) return;
    double duration_3_times = (append_time - created_time_) * 3;
    if (duration_3_times > Config::GetConfig()->duration_ * 1000 && duration_3_times < Config::GetConfig()->duration_ * 2 * 1000)
      data_.push_back(x);
  }
  // only append if append_time is in mid 1/3 time (10~20s if duration is 30s)
  void mid_time_append(double x) {
    // if (pct_lock) return;
    double append_time = SimpleRWCommand::GetCurrentMsTime();
    double duration_3_times = (append_time - created_time_) * 3;
    if (duration_3_times > Config::GetConfig()->duration_ * 1000 && duration_3_times < Config::GetConfig()->duration_ * 2 * 1000)
      data_.push_back(x);
  }
  void merge(Distribution &o) {
    for (int i = 0; i < o.count(); i++)
      data_.push_back(o.data_[i]);
  }
  size_t count() {
    return data_.size();
  }
  double recent_100_ave() { // only work when append only
    if (data_.size() == 0)
      return 0;
    if (data_.size() > 100)
      return recent_100_sum_ / 100;
    else
      return recent_100_sum_ / data_.size();
  }
  double pct(double pct) {
    verify(pct >= 0.0 - 1e-6 && pct <= 100.0 + 1e-6);
    // pct_lock = true;
    if (data_.size() == 0)
      return -1;
    sort(data_.begin(), data_.end());
    int pick = floor(data_.size() * pct);
    if (pick == data_.size())
      pick -= 1;
    return data_[pick];
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
  double ave() {
    if (data_.size() == 0)
      return -1;
    double sum = 0;
    for (int i = 0; i < data_.size(); i++)
      sum += data_[i];
    return sum / data_.size();
  }
  string statistics() {
    std::ostringstream oss;
    oss << std::setw(7) << "count" << std::setw(7) << count();
    oss << std::setw(7) << " 0pct" << std::setw(7) << std::fixed << std::setprecision(2) << pct(0.0);
    oss << std::setw(7) << "50pct" << std::setw(7) << std::fixed << std::setprecision(2) << pct(0.5);
    oss << std::setw(7) << "90pct" << std::setw(7) << std::fixed << std::setprecision(2) << pct(0.9);
    oss << std::setw(7) << "99pct" << std::setw(7) << std::fixed << std::setprecision(2) << pct(0.99);
    oss << std::setw(7) << "  ave" << std::setw(7) << std::fixed << std::setprecision(2) << ave();
    return oss.str();
  }
  string distribution() {
    std::ostringstream oss;
    for (int i = 0; i <= 100; i += 10) {
      // oss << i << "pct ";
      oss << std::setw(7) << std::fixed << std::setprecision(2) << pct(i / 100.0);
    }
    return oss.str();
  }
};

class Frequency {
  vector<int> keys_;
 public:
  void append(double x) {
    keys_.push_back(x);
  }
  void merge(Frequency &o) {
    for (int i = 0; i < o.count(); i++)
      keys_.push_back(o.keys_[i]);
  }
  size_t count() {
    return keys_.size();
  }
  string top_keys_pcts() {
    unordered_map<int, int> count_map;
    for (auto k: keys_) {
      count_map[k]++;
    }
    set<pair<int, int>> frequency;
    for (auto it: count_map) {
      frequency.insert(make_pair(-it.second, it.first));
    }
    std::stringstream ss;
    int i = 0;
    for (set<pair<int, int>>::iterator it = frequency.begin(); it != frequency.end() && i < 10; it++, i++) {
      ss << std::fixed << std::setprecision(6) << -it->first * 100.0 / count() << " (" << it->second << "), ";
    }
    return ss.str();
  }
};

class RevoveryCandidates {
  int maximal_ = -1;
  int total_write_ = 0;
  // <cmd_id, <cnt_for_this_key, is_write> >
  unordered_map<uint64_t, pair<int, bool>> candidates_;
  // <cmd_id, cnt_for_this_cmd_id>
  unordered_map<uint64_t, int> cmd_count_;
 public:
  RevoveryCandidates() {}
  void push_back(uint64_t cmd_id, bool is_write);
  bool remove(uint64_t cmd_id);
  bool has_appeared(uint64_t cmd_id);
  size_t size();
  int total_write();
  uint64_t id_of_candidate_to_recover();
};

class Witness {
  class WitnessLog {
   public:
    double time_;
    int operation_; // 0: push_back; 1: remove
    shared_ptr<Marshallable> cmd_;
    bool success_;
    int size_;
    WitnessLog(int operation, shared_ptr<Marshallable> cmd, bool success, int size):
      operation_(operation), cmd_(cmd), success_(success), size_(size) {
      time_ = SimpleRWCommand::GetCurrentMsTime();
    }
    void print(double init_time) {
      pair<int32_t, int32_t> cmd_id = SimpleRWCommand::GetCmdID(cmd_);
      uint64_t cmd_id_combined = SimpleRWCommand::GetCombinedCmdID(cmd_);
      if (operation_ == 0) {
        Log_info("Log %.2f size %d suc %d key %" PRId32 " push_back %" PRId32 " %" PRId32 " %" PRId64, time_ - init_time, size_, success_, SimpleRWCommand::GetKey(cmd_), cmd_id.first, cmd_id.second, cmd_id_combined);
      } else if (operation_ == 1) {
        Log_info("Log %.2f size %d suc %d key %" PRId32 " remove %" PRId32 " %" PRId32 " %" PRId64, time_ - init_time, size_, success_, SimpleRWCommand::GetKey(cmd_), cmd_id.first, cmd_id.second, cmd_id_combined);
      } else {
        verify(0);
      }
    }
  };
  bool belongs_to_leader_{false}; // i.e. This server can propose value
  unordered_map<key_t, RevoveryCandidates> candidates_;
  int witness_size_ = 0;
  Distribution witness_size_distribution_;
#ifdef WITNESS_LOG_DEBUG
  vector<WitnessLog> witness_log_;
#endif
 public:
  Witness() {};
  ~Witness() {};
  // return whether meet conflict, but not whether push_back success
  bool push_back(const shared_ptr<Marshallable>& cmd);
  // return how many cmd have been removed (cmd may be CMD_TPC_BATCH)
  int remove(const shared_ptr<Marshallable>& cmd);
  // return whether all cmds appeared before
  bool has_appeared(const shared_ptr<Marshallable>& cmd);
  void set_belongs_to_leader(bool belongs_to_leader);
  // return 50pct, 90pct, 99pct, ave of the witness_size_distribution_
  std::vector<double> witness_size_distribution();
#ifdef WITNESS_LOG_DEBUG
  void print_log();
#endif
};

class RecentAverage {
  vector<double> data_;
  int size_, pointer_ = 0;
  double sum = 0;
  bool filled_once_ = false;
 public:
  RecentAverage(int size): size_(size) {
    // intentionally left blank
  }
  void append(double x) {
    if (!filled_once_) {
      data_.push_back(x);
      pointer_++;
    } else {
      sum -= data_[pointer_];
      data_[++pointer_] = x;
    }
    sum += x;
    if (pointer_ == size_) {
      pointer_ = 0;
      filled_once_ = true;
    }
  }
  bool filled_once() {
    return filled_once_;
  }
  double ave() {
    // Log_info("RecentAverage ave %d %d", filled_once_, pointer_);
    verify(filled_once_ || pointer_ > 0);
    return filled_once_ ? sum / size_ : sum / pointer_;
  }
};

struct ResponseData {
  // pair<ver_t, ver_t> pos_of_this_pack;
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
  unordered_set<int64_t> curp_instance_commit_in_pool_;
  set<pair<double, shared_ptr<CurpPlusData>>> curp_instance_commit_pool_; // [CURP] TODO: This may can be optimized
  bool timeout_loop_started_ = false;
  CurpInstanceCommitTimeoutPool() {
    // Coroutine::CreateRun([this]() { 
    //   TimeoutLoop();
    // });
  }
  void TimeoutLoop();
  void AddTimeoutInstance(shared_ptr<CurpPlusData> instance, bool repeat_insert = false);
};

class CurpCoordinatorCommitFinishTimeoutPool {
  public:
    TxLogServer *sch_ = nullptr;
    // <key> exist in this pool iff there exist a Finish symbol related to <key> in process (either in wait_for_commit_events_pool_)
    unordered_set<key_t> commit_finish_in_pool_;
    // This pool manages all QuorumEvents that commits Finish along with original protocol, key is original protocol cmd_id
    unordered_map<int64_t, pair<key_t, shared_ptr<CurpDispatchQuorumEvent>>> wait_for_commit_events_pool_{};
    bool timeout_loop_started_ = false;
    CurpCoordinatorCommitFinishTimeoutPool(TxLogServer *sch): sch_(sch) {
      // Coroutine::CreateRun([this]() { 
      //   TimeoutLoop();
      // });
    };
    void TimeoutLoop();
    void DealWith(uint64_t cmd_id);
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
  virtual void Pause();
  virtual void Resume();

  // epoch related functions
  void TriggerUpgradeEpoch();
  void UpgradeEpochAck(parid_t par_id, siteid_t site_id, int res);
  virtual int32_t OnUpgradeEpoch(uint32_t old_epoch);

  // below are about CURP

  unordered_map<key_t, shared_ptr<CurpPlusDataCol>> curp_log_cols_{};
  pair<key_t, ver_t> curp_executed_garbage_collection_[100000];
  int curp_executed_garbage_collection_pointer_ = 0;
  CurpInstanceCommitTimeoutPool curp_instance_commit_timeout_pool_;
  CurpCoordinatorCommitFinishTimeoutPool curp_coordinator_commit_finish_timeout_pool_;
  unordered_map<int64_t, shared_ptr<ResponseData>> curp_response_storage_;

  // CURP countings
  int curp_fast_path_success_count_ = 0;
  int curp_coordinator_accept_count_ = 0;
  int original_protocol_submit_count_ = 0;

  int curp_fastpath_timeout_count_ = 0;
  int curp_wait_commit_timeout_count_ = 0;
  int curp_instance_commit_timeout_trigger_prepare_count_ = 0;
  int finish_countdown_count_ = 0;

  int curp_unique_original_cmd_id_ = 0;

  // CURP Timestamp debug
  Distribution cli2svr_dispatch, cli2svr_commit;
#ifdef LATENCY_DEBUG
  Distribution cli2preskip_begin_, cli2preskip_end_, cli2skip_begin_, cli2skip_end_;
  Distribution cli2leader_recv_, cli2leader_send_, cli2follower_recv_, cli2follower_send_, cli2commit_send_, cli2oncommit_;
  Distribution preskip_attemp_;
#endif

  // CURP commit finish in advance
  unordered_map<key_t, bool> curp_in_commit_finish_;

  // CURP coordinator reply/notify the client: cmd_id index
  // map<pair<int32_t, int32_t>, shared_ptr<CommitNotification>> executed_results_;
  vector<shared_ptr<CommitNotification>> commit_timeout_list_;
  int commit_timeout_solved_count_;

  unordered_map<key_t, int> finish_countdown_;

  // application k-v table for rw workload
  unordered_map<key_t, value_t> kv_table_;

  // [CURP] TODO: discard assigned_ since it is used to debug a old bug which have been solved
#ifdef CURP_INSTANCE_CREATED_ONLY_ONCE_CHECK
  map<pair<int, int>, bool> assigned_;
#endif

  double first_print_structure_size_time_ = 0;
  double last_print_structure_size_time_ = 0;

  int curp_double_commit_count_ = 0;

  // For checksum
  unordered_map<key_t, value_t> database_;
  int database_operation_count_ = 0;

  void ApplyToDatabase(shared_ptr<Marshallable> cmd) {
    SimpleRWCommand parsed_cmd = SimpleRWCommand(cmd);
    // Log_info("Apply Write %d key %d value %d", parsed_cmd.IsWrite(), parsed_cmd.key_, parsed_cmd.value_);
    if (parsed_cmd.IsWrite()) {
      database_[parsed_cmd.key_] = parsed_cmd.value_;
      database_operation_count_++;
    }
  }

  uint32_t ChecksumXor() {
    Log_info("database_operation_count_ %d", database_operation_count_);
    uint32_t checksum = 0;
    for (const auto& kv : database_) {
        checksum ^= static_cast<uint32_t>(kv.first);
        checksum ^= static_cast<uint32_t>(kv.second);
    }
    return checksum;
  }

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

  // This used for garbage collection / evaluation data structure grows over time
  void PrintStructureSize();

  // below are about rule

  Witness witness_;

  // For Rule usage
  void OnRuleSpeculativeExecute(const shared_ptr<Marshallable>& cmd,
                                bool_t* accepted,
                                value_t* result,
                                bool_t* is_leader);

  void OriginalPathUnexecutedCmdConflictPlaceHolder(const shared_ptr<Marshallable>& cmd);

  void RuleWitnessGC(const shared_ptr<Marshallable>& cmd);

#ifdef ZERO_OVERHEAD
  virtual bool ConflictWithOriginalUnexecutedLog(const shared_ptr<Marshallable>& cmd) {
    // This function should be overrided by the deriviated class (replica server)
    assert(0);
    return false;
  }
#endif
  // void OnRulePrepare(uint32_t epoch,
  //                    ballot_t ballot,
  //                    value_t* result,
  //                    int* acc_ballot,
  //                    Witness* pool);
  
  // void OnRuleAccept(uint32_t epoch,
  //                   ballot_t ballot,
  //                   Witness pool,
  //                   value_t* result);

  // void OnRuleCommit(uint32_t epoch,
  //                   Witness pool);
};

} // namespace janus
