#include "../__dep__.h"
#include "../constants.h"
#include "frame.h"
#include "coordinator.h"
#include "server.h"
#include "service.h"
#include "commo.h"

namespace janus {

REG_FRAME(MODE_RAFT, vector<string>({"raft"}), RaftFrame);

RaftFrame::RaftFrame(int mode): Frame(mode) {

}

Executor* RaftFrame::CreateExecutor(cmdid_t cmd_id, TxLogServer *sched) {
  return nullptr;
}

Coordinator* RaftFrame::CreateCoordinator(cooid_t coo_id,
                                          Config *config,
                                          int benchmark,
                                          ClientControlServiceImpl *ccsi,
                                          uint32_t id,
                                          shared_ptr<TxnRegistry> txn_reg) {
  verify(config != nullptr);
  CoordinatorRaft *coo;
  coo = new CoordinatorRaft(coo_id,
                            benchmark,
                            ccsi,
                            id);
  coo->frame_ = this;
  verify(commo_ != nullptr);
  coo->commo_ = commo_;
  coo->slot_hint_ = &slot_hint_;
  coo->slot_id_ = slot_hint_++;
  coo->n_replica_ = config->GetPartitionSize(site_info_->partition_id_);
  coo->loc_id_ = this->site_info_->locale_id;
  verify(coo->n_replica_ != 0); // TODO
  Log_debug("create new multi-paxos coord, coo_id: %d", (int) coo->coo_id_);
  return coo;
}

TxLogServer* RaftFrame::CreateScheduler() {
  TxLogServer *sch = nullptr;
  sch = new RaftServer();
  sch->frame_ = this;
  return sch;
}

Communicator* RaftFrame::CreateCommo(PollMgr *poll) {
  if (commo_ == nullptr) {
    commo_ = new RaftCommo(poll);
  }
  return commo_;
}

vector<rrr::Service *> RaftFrame::CreateRpcServices(uint32_t site_id,
                                                    TxLogServer *rep_sched,
                                                    rrr::PollMgr *poll_mgr,
                                                    ServerControlServiceImpl *scsi) {
  auto config = Config::GetConfig();
  auto result = std::vector<Service *>();
  switch (config->replica_proto_) {
    case MODE_RAFT:result.push_back(new RaftServiceImpl(rep_sched));
    default:break;
  }
  return result;
}


}