#include "RW_command.h"

#include "bench/rw/procedure.h"
#include "bench/rw/workload.h"
#include "classic/tpc_command.h"

namespace janus {

static int volatile x =
    MarshallDeputy::RegInitializer(MarshallDeputy::CMD_KV,
                                     [] () -> Marshallable* {
                                       return new SimpleRWCommand;
                                     });

SimpleRWCommand::SimpleRWCommand(): Marshallable(MarshallDeputy::CMD_KV) {
  //Log_info("[copilot+] SimpleRWCommand Empty created");
  type_ = CmdType::NoOp;
  key_ = 0;
  value_ = 0;
}

SimpleRWCommand::SimpleRWCommand(shared_ptr<Marshallable> cmd): Marshallable(MarshallDeputy::CMD_KV) {
  //Log_info("[copilot+] SimpleRWCommand created");
  shared_ptr<vector<shared_ptr<SimpleCommand>>> sp_vec_piece{nullptr};
  if (cmd->kind_ == MarshallDeputy::CMD_TPC_COMMIT) {
    shared_ptr<TpcCommitCommand> tpc_cmd = dynamic_pointer_cast<TpcCommitCommand>(cmd);
    VecPieceData *cmd_cast = (VecPieceData*)(tpc_cmd->cmd_.get());
    sp_vec_piece = cmd_cast->sp_vec_piece_data_;
  } else if (cmd->kind_ == MarshallDeputy::CMD_VEC_PIECE) {
    shared_ptr<VecPieceData> cmd_cast = dynamic_pointer_cast<VecPieceData>(cmd);
    sp_vec_piece = cmd_cast->sp_vec_piece_data_;
  } else {
    verify(0);
  }
  shared_ptr<TxPieceData> vector0 = *(sp_vec_piece->begin());
  TxWorkspace tx_ws = vector0->input;
  std::map<int32_t, mdb::Value> kv_map = *(tx_ws.values_);
  auto raw_type = vector0->type_;
  if (vector0->type_ == RW_BENCHMARK_R_TXN || vector0->type_ == RW_BENCHMARK_R_TXN_0) {
    type_ = CmdType::Read;
    key_ = kv_map[0].get_i32();
    value_ = 0;
  } else if (vector0->type_ == RW_BENCHMARK_W_TXN || vector0->type_ == RW_BENCHMARK_W_TXN_0) {
    type_ = CmdType::Write;
    key_ = kv_map[0].get_i32();
    value_ = kv_map[1].get_i32();
  } else {
    //Log_info("[copilot+][error] type read from cmd: %d", vector0->type_);
    verify(0);
  }
  // key_t key = (*(*(((VecPieceData*)(dynamic_pointer_cast<TpcCommitCommand>(cmd)->cmd_.get()))->sp_vec_piece_data_->begin()))->input.values_)[0].get_i32();
}

// SimpleRWCommand::SimpleRWCommand(const SimpleRWCommand &o): Marshallable(o.kind_) {
//   type_ = o.type_;
//   key_ = o.key_;
//   value_ = o.value_;
// }


string SimpleRWCommand::cmd_to_string() {
  //Log_info("[copilot+] enter cmd_to_string of %p", (void*)(this));
  //Log_info("[copilot+] cmd_type=%d", type_);
  if (CmdType::NoOp == type_)
    return string("NoOp");
  else if (CmdType::Read == type_)
    return string("Read k=" + to_string(key_));
  else if (CmdType::Write == type_)
    return string("Write k=" + to_string(key_) + " v=" + to_string(value_));
  else
    verify(0);
}

Marshal& SimpleRWCommand::ToMarshal(Marshal& m) const {
  m << type_;
  m << key_;
  m << value_;
  return m;
}

Marshal& SimpleRWCommand::FromMarshal(Marshal& m) {
  m >> type_;
  m >> key_;
  m >> value_;
  return m;
}


}