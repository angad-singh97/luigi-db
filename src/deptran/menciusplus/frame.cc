#include "../__dep__.h"
#include "../constants.h"
#include "frame.h"
#include "exec.h"
#include "coordinator.h"
#include "server.h"
#include "service.h"
#include "commo.h"
#include "config.h"

namespace janus {

REG_FRAME(MODE_MENCIUS_PLUS, vector<string>({"mencius_plus"}), MenciusPlusFrame);

MenciusPlusFrame::MenciusPlusFrame(int mode) : Frame(mode) {

}

Executor *MenciusPlusFrame::CreateExecutor(cmdid_t cmd_id, TxLogServer *sched) {
  Executor *exec = new MenciusPlusExecutor(cmd_id, sched);
  return exec;
}

Coordinator *MenciusPlusFrame::CreateCoordinator(cooid_t coo_id,
                                                Config *config,
                                                int benchmark,
                                                ClientControlServiceImpl *ccsi,
                                                uint32_t id,
                                                shared_ptr<TxnRegistry> txn_reg) {
  verify(config != nullptr);
  CoordinatorMenciusPlus *coo;
  coo = new CoordinatorMenciusPlus(coo_id,
                                  benchmark,
                                  ccsi,
                                  id);
  coo->frame_ = this;
  verify(commo_ != nullptr);
  coo->commo_ = commo_;
  slot_id_ = slot_hint_ + site_info_->id;
  coo->slot_hint_ = &slot_id_;
  coo->slot_id_ = slot_id_;
  coo->n_replica_ = config->GetPartitionSize(site_info_->partition_id_);
  slot_hint_ += coo->n_replica_;
  coo->loc_id_ = this->site_info_->locale_id;
  verify(this->sch_ != nullptr);
  coo->sch_ = this->sch_;
  verify(coo->n_replica_ != 0); // TODO
  return coo;
}

TxLogServer *MenciusPlusFrame::CreateScheduler() {
  MenciusPlusServer *sch = nullptr;
  sch = new MenciusPlusServer();
  sch->frame_ = this;
  this->sch_ = sch;
  return sch;
}

Communicator *MenciusPlusFrame::CreateCommo(PollMgr *poll) {
  // We only have 1 instance of MenciusPlusFrame object that is returned from
  // GetFrame method. MenciusPlusCommo currently seems ok to share among the
  // clients of this method.
  if (commo_ == nullptr) {
    commo_ = new MenciusPlusCommo(poll);
  }
  return commo_;
}

vector<rrr::Service *>
MenciusPlusFrame::CreateRpcServices(uint32_t site_id,
                                   TxLogServer *rep_sched,
                                   rrr::PollMgr *poll_mgr,
                                   ServerControlServiceImpl *scsi) {
  auto config = Config::GetConfig();
  auto result = std::vector<Service *>();
  switch (config->replica_proto_) {
    case MODE_MENCIUS_PLUS:result.push_back(new MenciusPlusServiceImpl(rep_sched));
    default:break;
  }
  return result;
}

} // namespace janus;
