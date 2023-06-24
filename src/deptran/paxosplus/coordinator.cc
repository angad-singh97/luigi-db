
#include "../__dep__.h"
#include "../constants.h"
#include "coordinator.h"
#include "commo.h"

namespace janus {

CoordinatorMultiPaxosPlus::CoordinatorMultiPaxosPlus(uint32_t coo_id,
                                             int32_t benchmark,
                                             ClientControlServiceImpl* ccsi,
                                             uint32_t thread_id)
    : Coordinator(coo_id, benchmark, ccsi, thread_id) {
}
                                    
void CoordinatorMultiPaxosPlus::FastSubmit(shared_ptr<Marshallable>& cmd,
                                            bool_t& accepted,
                                            shared_ptr<Position>& pos,
                                            value_t& result,
                                            const std::function<void()>& commit_callback,
                                            const std::function<void()>& exe_callback) {
  std::lock_guard<std::recursive_mutex> lock(mtx_);
  Log_info("[copilot+] [4+] enter CopilotPlusCoordinator::FastSubmit");

  shared_ptr<SimpleRWCommand> parsed_cmd_ = make_shared<SimpleRWCommand>(cmd);
  key_t key = dynamic_pointer_cast<SimpleRWCommand>(parsed_cmd_)->key_;
  bool_t fast_path_validation = sch_->check_fast_path_validation(key);
  if (!fast_path_validation) {
    accepted = false;
    pos.get()->set(0, -1);
    result = 0;
  } else {
    accepted = true;
    if (parsed_cmd_->type_ == SimpleRWCommand::CmdType::Read) {
      pos.get()->set(0, -1);
      result = sch_->read(key);
    } else if (parsed_cmd_->type_ == SimpleRWCommand::CmdType::Write) {
      slotid_t new_slot_pos = sch_->append_cmd(key, cmd);
      pos.get()->set(0, new_slot_pos);
      result = 1;
    } else {
      verify(0);
    }
  }
  shared_ptr<IntEvent> sq_quorum = commo()->ForwardResultToCoordinator(par_id_, pos, cmd, accepted);
  // TODO
  commit_callback_ = commit_callback;
  commit_callback_();
}


} // namespace janus
