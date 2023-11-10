#pragma once

#include "../__dep__.h"
#include "../constants.h"
#include "../scheduler.h"
#include "../classic/tpc_command.h"
#include "../copilot/server.h"
#include <stack>
#include "../RW_command.h"

#define REVERSE(p) (1 - (p))

namespace janus {

/* ****************************************** comment for avoid conflict

const status_t FLAG_TAKEOVER = 0x80000000;
const status_t CLR_FLAG_TAKEOVER = (~FLAG_TAKEOVER);
#define GET_STATUS(s) ((s) & CLR_FLAG_TAKEOVER)
#define GET_TAKEOVER(s) ((s) & FLAG_TAKEOVER)
#define SET_STATUS(s_old, s_new) ((s_old) = (s_new) | GET_TAKEOVER(s_old))

enum Status : status_t { NOT_ACCEPTED = 0, FAST_ACCEPTED, FAST_ACCEPTED_EQ, ACCEPTED, COMMITED, EXECUTED };
const size_t n_status = 5;

*************************************************** */

class FrontCopilotPlusData {
 public:
  enum Status {INIT, PRE_EXECUTED, OVER_WRITTEN, COMMITTED};
 private:
  shared_ptr<Marshallable> cmd_{nullptr};
  bool_t no_op_ = false;
 public:
  Status status_ = Status::INIT;
  FrontCopilotPlusData(shared_ptr<Marshallable> cmd): cmd_(cmd), no_op_(nullptr == cmd) {}
  FrontCopilotPlusData(shared_ptr<Marshallable> cmd, bool_t no_op): cmd_(cmd), no_op_(no_op) {}
};

class CopilotPlusData {
 public:
  shared_ptr<Marshallable>  cmd{nullptr};  // command
  slotid_t                  dep_id;  // dependency
  uint8_t                   is_pilot;
  slotid_t                  slot_id;  // position
  ballot_t                  ballot;  // ballot
  status_t                  status;  // status
  int                       low, dfn;  //tarjan
  SharedIntEvent            cmit_evt{};
  /***************************************PLUS Begin***********************************************************/
  key_t key_;
  std::vector<FrontCopilotPlusData> attached_; // include cmd in position 0
  CopilotPlusData(shared_ptr<Marshallable> cmd, slotid_t dep_id, uint8_t is_pilot, slotid_t slot_id, ballot_t ballot,
                  status_t status, int low, int dfn): cmd(cmd), dep_id(dep_id), is_pilot(is_pilot), slot_id(slot_id),
                  ballot(ballot), status(status), low(low), dfn(dfn) {
      SimpleRWCommand parsed_cmd = cmd == nullptr ? SimpleRWCommand() : SimpleRWCommand(cmd);
      key_ = parsed_cmd.key_;
      attached_.push_back(FrontCopilotPlusData(cmd));
    }
  /***************************************PLUS End***********************************************************/
};

struct CopilotPlusLogInfo {
  std::map<slotid_t, shared_ptr<CopilotPlusData> > logs;
  slotid_t current_slot = 0;
  slotid_t min_active_slot = 1; // anything before (lt) this slot is freed
  slotid_t max_executed_slot = 0;
  slotid_t max_committed_slot = 0;
  slotid_t max_accepted_slot = 0;
  slotid_t max_dep = 0;
  SharedIntEvent max_cmit_evt{};
};

class CopilotPlusServer : public TxLogServer {
  using copilot_stack_t = std::stack<shared_ptr<CopilotPlusData> >;
  using visited_map_t = std::map<shared_ptr<CopilotPlusData>, bool>;
 private:
  uint16_t id_;
  bool isPilot_ = false;
  bool isCopilot_ = false;

  void setIsPilot(bool isPilot);
  void setIsCopilot(bool isCopilot);
  const char* toString(uint8_t is_pilot);

 private:
  std::vector<CopilotPlusLogInfo> log_infos_;

  SharedIntEvent pingpong_event_{};
  bool pingpong_ok_;
  uint64_t last_ready_time_ = 0;
  uint64_t n_timeout = 0;

 public:
  /***************************************PLUS Begin***********************************************************/
  std::map<key_t, slotid_t> lastest_slot_map_[2];
  // slotid_t find_key(key_t key, uint8_t is_pilot);
  // bool check_slot_vector_last_committed(slotid_t slot, uint8_t is_pilot);
  // slotid_t push_back_cmd_to_slot(slotid_t slot, uint8_t is_pilot, shared_ptr<Marshallable>& cmd);
  /***************************************PLUS End***********************************************************/

  CopilotPlusServer(Frame *frame);
  ~CopilotPlusServer() {}

  shared_ptr<CopilotPlusData> GetInstance(slotid_t slot, uint8_t is_pilot);
  std::pair<slotid_t, uint64_t> PickInitSlotAndDep();
  slotid_t GetMaxCommittedSlot(uint8_t is_copilot);
  bool WaitMaxCommittedGT(uint8_t is_pilot, slotid_t slot, int timeout=0);
  
  /**
   * If the log entry has been executed (in another log), mark it as EXECUTED,
   * and update max_committed_slot and max_executed_slot accordingly
   * 
   * @param ins log entry to be check
   * @return true if executed, false otherwise
   */
  bool EliminateNullDep(shared_ptr<CopilotPlusData>& ins);
  bool AllDepsEliminated(uint8_t is_pilot, slotid_t dep_id);

  void Setup();

  // Wait for Ping-Pong signal from another pilot, or timeout and proceed to FastAccept
  void WaitForPingPong();
  bool WillWait(int& time_to_wait) const;

  void OnForward(shared_ptr<Marshallable>& cmd,
                 const function<void()> &cb);

  void OnPrepare(const uint8_t& is_pilot,
                 const uint64_t& slot,
                 const ballot_t& ballot,
                 const struct DepId& dep_id,
                 MarshallDeputy* ret_cmd,
                 ballot_t* max_ballot,
                 uint64_t* dep,
                 status_t* status,
                 const function<void()> &cb);

  void OnFastAccept(const uint8_t& is_pilot,
                    const uint64_t& slot,
                    const ballot_t& ballot,
                    const uint64_t& dep,
                    shared_ptr<Marshallable>& cmd,
                    const struct DepId& dep_id,
                    const uint64_t& commit_finish,
                    ballot_t* max_ballot,
                    uint64_t* ret_dep,
                    bool_t* finish_accept,
                    uint64_t* finish_ver,
                    const function<void()> &cb);

  void OnAccept(const uint8_t& is_pilot,
                const uint64_t& slot,
                const ballot_t& ballot,
                const uint64_t& dep,
                shared_ptr<Marshallable>& cmd,
                const struct DepId& dep_id,
                ballot_t* max_ballot,
                const function<void()> &cb);

  void OnCommit(const uint8_t& is_pilot,
                const uint64_t& slot,
                const uint64_t& dep,
                shared_ptr<Marshallable>& cmd);
 private:
  bool executeCmd(shared_ptr<CopilotPlusData>& ins);
  bool executeCmds(shared_ptr<CopilotPlusData>& ins);
  void waitAllPredCommit(shared_ptr<CopilotPlusData>& ins);
  void waitPredCmds(shared_ptr<CopilotPlusData>& ins, shared_ptr<visited_map_t> map);
  bool findSCC(shared_ptr<CopilotPlusData>& root);
  bool strongConnect(shared_ptr<CopilotPlusData>& ins, int* index);
  bool checkAllDepExecuted(uint8_t is_pilot, slotid_t start, slotid_t end);
  // void strongConnect(shared_ptr<CopilotPlusData>& ins, int* index, copilot_stack_t *stack);
  void updateMaxExecSlot(shared_ptr<CopilotPlusData>& ins);
  void updateMaxAcptSlot(CopilotPlusLogInfo& log_info, slotid_t slot);
  void updateMaxCmtdSlot(CopilotPlusLogInfo& log_info, slotid_t slot);
  void removeCmd(CopilotPlusLogInfo& log_info, slotid_t slot);
  copilot_stack_t stack_;

  bool isExecuted(shared_ptr<CopilotPlusData>& ins);
  bool allCmdComitted(shared_ptr<Marshallable> batch_cmd);
};

} //namespace janus