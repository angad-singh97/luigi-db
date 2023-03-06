#pragma once

#include "../__dep__.h"
#include "../frame.h"
#include "commo.h"
#include "server.h"
#include "service.h"
#include "coordinator.h"

namespace janus {
class CopilotPlusFrame: public Frame {
private:
  slotid_t slot_hint_ = 1;
  slotid_t slot_id_ = 0;
  
  CopilotPlusCommo *commo_ = nullptr;
  CopilotPlusServer *svr_ = nullptr;

 public:
  CopilotPlusFrame(int mode);
  virtual ~CopilotPlusFrame();

  Coordinator *CreateCoordinator(cooid_t coo_id,
                                 Config *config,
                                 int benchmark,
                                 ClientControlServiceImpl *ccsi,
                                 uint32_t thread_id,
                                 shared_ptr<TxnRegistry> txn_reg) override;
  
  TxLogServer *CreateScheduler() override;
  
  Communicator *CreateCommo(PollMgr *poll = nullptr) override;
  
  vector<rrr::Service *> CreateRpcServices(uint32_t site_id,
                                           TxLogServer *rep_svr,
                                           rrr::PollMgr *poll_mgr,
                                           ServerControlServiceImpl *scsi) override;
};
}
