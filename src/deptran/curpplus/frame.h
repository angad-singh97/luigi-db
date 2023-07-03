#pragma once

#include <deptran/communicator.h>
#include "../frame.h"
#include "../constants.h"
#include "commo.h"
#include "server.h"

namespace janus {

class CurpPlusFrame : public Frame {
 private:
  slotid_t slot_hint_ = 1;
  CurpPlusServer *sch_ = nullptr;
  CurpPlusCommo *commo_ = nullptr;
 public:
  CurpPlusFrame(int mode);
  Coordinator *CreateCoordinator(cooid_t coo_id,
                                 Config *config,
                                 int benchmark,
                                 ClientControlServiceImpl *ccsi,
                                 uint32_t id,
                                 shared_ptr<TxnRegistry> txn_reg) override;
  TxLogServer *CreateScheduler() override;
  Communicator *CreateCommo(PollMgr *poll = nullptr) override;
  vector<rrr::Service *> CreateRpcServices(uint32_t site_id,
                                           TxLogServer *dtxn_sched,
                                           rrr::PollMgr *poll_mgr,
                                           ServerControlServiceImpl *scsi) override;
};

} // namespace janus
