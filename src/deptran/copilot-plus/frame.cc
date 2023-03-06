#include "../__dep__.h"
#include "../constants.h"
#include "frame.h"

namespace janus {

REG_FRAME(MODE_COPILOT_PLUS, vector<string>({"copilot_plus"}), CopilotPlusFrame);

CopilotPlusFrame::CopilotPlusFrame(int mode): Frame(mode){

}

CopilotPlusFrame::~CopilotPlusFrame() {

}

Coordinator* CopilotPlusFrame::CreateCoordinator(cooid_t coo_id,
                                 Config *config,
                                 int benchmark,
                                 ClientControlServiceImpl *ccsi,
                                 uint32_t thread_id,
                                 shared_ptr<TxnRegistry> txn_reg) {
  verify(config != nullptr);
  CopilotPlusCoordinator *coo = new CopilotPlusCoordinator(coo_id, benchmark, ccsi, thread_id);
  Log_info("CopilotPlusCoordinator %p created", (void*)this);
  coo->frame_ = this;
  coo->commo_ = commo_;
  return coo;
};
  
TxLogServer* CopilotPlusFrame::CreateScheduler() {
  verify(svr_ == nullptr);
  svr_ = new CopilotPlusServer(this);
  return svr_;
}

Communicator* CopilotPlusFrame::CreateCommo(PollMgr *poll) {
  if (commo_ == nullptr) {
    commo_ = new CopilotPlusCommo(poll);
  }
  return commo_;
}

vector<rrr::Service *> CopilotPlusFrame::CreateRpcServices(uint32_t site_id,
                                        TxLogServer *rep_svr,
                                        rrr::PollMgr *poll_mgr,
                                        ServerControlServiceImpl *scsi) {
  auto config = Config::GetConfig();
  auto result = vector<Service *>();
  switch (config->replica_proto_) {
  case MODE_COPILOT_PLUS:
    result.push_back(new CopilotPlusServiceImpl(rep_svr));
    break;
  default:
    break;
  }
  return result;
}

}