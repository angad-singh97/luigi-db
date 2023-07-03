#include "../__dep__.h"
#include "../constants.h"
#include "frame.h"
#include "coordinator.h"
#include "server.h"
#include "service.h"
#include "commo.h"
#include "config.h"

namespace janus {

REG_FRAME(MODE_CURP_PLUS, vector<string>({"curp_plus"}), CurpPlusFrame);

/*template<typename D>
struct automatic_register {
 private:
  struct exec_register {
    exec_register() {
      D::do_it();
    }
  };
  // will force instantiation of definition of static member
  template<exec_register&> struct ref_it { };

  static exec_register register_object;
  static ref_it<register_object> referrer;
};

template<typename D> typename automatic_register<D>::exec_register
    automatic_register<D>::register_object;

struct foo : automatic_register<foo> {
  static void do_it() {
    REG_FRAME(MODE_MULTI_PAXOS, vector<string>({"paxos"}), CurpPlusFrame);
  }
};*/

CurpPlusFrame::CurpPlusFrame(int mode) : Frame(mode) {

}

Coordinator *CurpPlusFrame::CreateCoordinator(cooid_t coo_id,
                                                Config *config,
                                                int benchmark,
                                                ClientControlServiceImpl *ccsi,
                                                uint32_t id,
                                                shared_ptr<TxnRegistry> txn_reg) {
  verify(config != nullptr);
  CoordinatorCurpPlus *coo;
  coo = new CoordinatorCurpPlus(coo_id,
                                  benchmark,
                                  ccsi,
                                  id);
  coo->frame_ = this;
  verify(commo_ != nullptr);
  coo->commo_ = commo_;
  coo->n_replica_ = config->GetPartitionSize(site_info_->partition_id_);
  coo->loc_id_ = this->site_info_->locale_id;
  coo->sch_ = sch_;
  verify(coo->n_replica_ != 0); // TODO
  Log_debug("create new multi-paxos coord, coo_id: %d", (int) coo->coo_id_);
  return coo;
}

TxLogServer *CurpPlusFrame::CreateScheduler() {
  if (sch_ == nullptr) {
    sch_ = new CurpPlusServer();
    sch_->frame_ = this;
  } else {
    verify(0);
  }
  Log_debug("[CURP] create curp+ sched loc: %d", this->site_info_->locale_id);
  return sch_;
}

Communicator *CurpPlusFrame::CreateCommo(PollMgr *poll) {
  // We only have 1 instance of CurpPlusFrame object that is returned from
  // GetFrame method. CurpPlusCommo currently seems ok to share among the
  // clients of this method.
  if (commo_ == nullptr) {
    commo_ = new CurpPlusCommo(poll);
  }
  return commo_;
}

vector<rrr::Service *>
CurpPlusFrame::CreateRpcServices(uint32_t site_id,
                                   TxLogServer *rep_sched,
                                   rrr::PollMgr *poll_mgr,
                                   ServerControlServiceImpl *scsi) {
  auto config = Config::GetConfig();
  auto result = std::vector<Service *>();
  Log_info("[CURP] start CurpPlusServiceImpl");
  switch (config->replica_proto_) {
    case MODE_CURP_PLUS:result.push_back(new CurpPlusServiceImpl(rep_sched));
    default:break;
  }
  return result;
}

} // namespace janus;
