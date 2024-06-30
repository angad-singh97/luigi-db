#pragma once

#include "../frame.h"
#include "../constants.h"
#include "commo.h"
#include "server.h"

namespace janus {

class RaftFrame : public Frame {

 private:
  slotid_t slot_hint_ = 1;

 public:

  RaftFrame(int mode);

  Executor* CreateExecutor(cmdid_t cmd_id, TxLogServer *sched) override;

  Coordinator* CreateCoordinator(cooid_t coo_id,
                                 Config *config,
                                 int benchmark,
                                 ClientControlServiceImpl *ccsi,
                                 uint32_t id,
                                 shared_ptr<TxnRegistry> txn_reg) override;

  TxLogServer* CreateScheduler() override;

  Communicator* CreateCommo(PollMgr *poll = nullptr) override;

  vector<rrr::Service *> CreateRpcServices(uint32_t site_id,
                                           TxLogServer *rep_sched,
                                           rrr::PollMgr *poll_mgr,
                                           ServerControlServiceImpl *scsi) override;

};

}