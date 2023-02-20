#include "../config.h"
#include "../multi_value.h"
#include "../procedure.h"
#include "../txn_reg.h"
#include "scheduler.h"
#include "../rcc_rpc.h"
#include "../bench/rw/workload.h" //<copilot+ debug>

namespace janus {
bool SchedulerNone::Dispatch(cmdid_t cmd_id, shared_ptr<Marshallable> cmd,
								TxnOutput& ret_output) {
	Log_debug("Dispatch the request to the correct protocol on the server side");
	Log_debug("[copilot+] enter SchedulerNone::Dispatch");

#ifdef COPILOTP_KV_DEBUG
  /****************** copilot+ kv debug begin **********************/ 
	VecPieceData *cmd_cast_test = (VecPieceData*)(cmd.get());
	shared_ptr<vector<shared_ptr<SimpleCommand>>> sp_vec_piece = cmd_cast_test->sp_vec_piece_data_;


	shared_ptr<vector<shared_ptr<TxPieceData>>> t1 = sp_vec_piece;
	auto t2 = (*(t1->begin()))->input.values_;
	Log_info("[copilot+] map size=%d", t2->size());

	for (auto it = t1->begin(); it != t1->end(); it++){
		SimpleCommand* cmd_cast_ = (SimpleCommand*)it->get();
		auto cmd_input = (*it)->input.values_;
		for (auto it2 = cmd_input->begin(); it2 != cmd_input->end(); it2++) {
			Log_info("[copilot+] key=%d value=%d", it2->first, it2->second.get_i32());
		}

		Log_info("[copilot+] input.values->size()=%d", cmd_cast_->input.values_->size());
		// if (cmd_cast_->type_ == RW_BENCHMARK_R_TXN)
		// 	Log_info("[copilot+] READ key=%d", (*cmd_cast_->input.values_)[0].get_i32());
		// else
		// 	Log_info("[copilot+] WRITE key=%d value=%d", (*cmd_cast_->input.values_)[0].get_i32(), (*cmd_cast_->input.values_)[1].get_i32());
	}
	
  /****************** copilot+ kv debug end **********************/
#endif

	auto sp_tx = dynamic_pointer_cast<TxClassic>(GetOrCreateTx(cmd_id));
	DepId di;
	di.str = "dep";
	di.id = 0;
	SchedulerClassic::Dispatch(cmd_id, di, cmd, ret_output);
	sp_tx->fully_dispatched_->Wait();
	OnCommit(cmd_id, di, SUCCESS);  // it waits for the command to be executed
	Log_debug("[copilot+] exit SchedulerNone::Dispatch");
	return true;
	
}

} // namespace janus
