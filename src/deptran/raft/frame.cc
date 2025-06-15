#include "../__dep__.h"
#include "../constants.h"
#include "frame.h"
#include "exec.h"
#include "coordinator.h"
#include "server.h"
#include "service.h"
#include "commo.h"
#include "config.h"
#include "test.h"
// #include "../kv/server.h"

namespace janus {

REG_FRAME(MODE_RAFT, vector<string>({"raft"}), RaftFrame);

/*
template<typename D>
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
    REG_FRAME(MODE_FPGA_RAFT, vector<string>({"fpga_raft"}), RaftFrame);
  }
};*/

#ifdef RAFT_TEST_CORO
std::recursive_mutex RaftFrame::raft_test_mutex_;
std::shared_ptr<Coroutine> RaftFrame::raft_test_coro_ = nullptr;
uint16_t RaftFrame::n_replicas_ = 0;
map<siteid_t, RaftFrame*> RaftFrame::frames_ = {};
bool RaftFrame::all_sites_created_s = false;
bool RaftFrame::tests_done_ = false;
static bool static_vars_initialized = false;
#endif

RaftFrame::RaftFrame(int mode) : Frame(mode) {
#ifdef RAFT_TEST_CORO
  std::lock_guard<std::recursive_mutex> lock(raft_test_mutex_);
  if (!static_vars_initialized) {
    Log_info("Initializing Raft test static variables");
    Log_info("Creating Raft test coroutine");
    Log_info("Setting initial number of replicas to 0");
    Log_info("Initializing empty frames map for tracking Raft replicas");
    Log_info("Setting all_sites_created flag to false");
    Log_info("Setting tests_done flag to false");
    static_vars_initialized = true;
  }
#endif
}

Executor *RaftFrame::CreateExecutor(cmdid_t cmd_id, TxLogServer *sched) {
  Executor *exec = new RaftExecutor(cmd_id, sched);
  return exec;
}

Coordinator *RaftFrame::CreateCoordinator(cooid_t coo_id,
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
  /* TODO: remove when have a class for common data */
  verify(svr_ != nullptr);
  coo->svr_ = this->svr_;
  coo->slot_hint_ = &slot_hint_;
  coo->slot_id_ = slot_hint_++;
  coo->n_replica_ = config->GetPartitionSize(site_info_->partition_id_);
  coo->loc_id_ = this->site_info_->locale_id;
  verify(coo->n_replica_ != 0); // TODO
  Log_debug("create new fpga raft coord, coo_id: %d", (int) coo->coo_id_);
  return coo;
}

TxLogServer *RaftFrame::CreateScheduler() {
  if(svr_ == nullptr)
  {
    svr_ = new RaftServer(this);
  }
  else
  {
    verify(0) ;
  }
  Log_debug("create new fpga raft sched loc: %d", this->site_info_->locale_id);

#ifdef RAFT_TEST_CORO
  raft_test_mutex_.lock();
  // verify(n_replicas_ < 5);
  const auto& site_id = this->site_info_->id;
  frames_[site_id] = this;
  int n_sites = 5;
  // Safely check if shard config exists and is true
  bool is_sharded = false;
  try {
    if (Config::GetConfig()->yaml_config_["lab"] && 
        Config::GetConfig()->yaml_config_["lab"]["shard"]) {
      is_sharded = Config::GetConfig()->yaml_config_["lab"]["shard"].as<bool>();
    }
  } catch (const YAML::Exception& e) {
    Log_info("No shard config found, defaulting to non-sharded mode");
  }
  
  if (is_sharded) {
    int n_partitions = Config::GetConfig()->GetNumPartition();
    verify(n_partitions==2);
    n_sites = 5*n_partitions; 
  }
  if (frames_.size() == n_sites) {
    all_sites_created_s = true;
  }
  raft_test_mutex_.unlock();
#endif

  return svr_ ;
}

Communicator *RaftFrame::CreateCommo(PollMgr *poll) {
  // We only have 1 instance of RaftFrame object that is returned from
  // GetFrame method. RaftCommo currently seems ok to share among the
  // clients of this method.
  if (commo_ == nullptr) {
    commo_ = new RaftCommo(poll);
  }

  #ifdef RAFT_TEST_CORO
  raft_test_mutex_.lock();
  
  // Register this frame if not already registered
  if (frames_.find(site_info_->id) == frames_.end()) {
    frames_[site_info_->id] = this;
  }
  
  // Only site 0 creates and manages the test coroutine
  if (site_info_->id == 0) {
    // Wait until all frames are registered
    while (frames_.size() < 5) {
      raft_test_mutex_.unlock();
      sleep(0.1);
      raft_test_mutex_.lock();
    }
    
    // Only create test coroutine if it doesn't exist yet
    if (raft_test_coro_.get() == nullptr) {
      Log_debug("Creating Raft test coroutine");
      raft_test_coro_ = Coroutine::CreateRun([this] () {
        // Yield until all communicators are initialized
        Coroutine::CurrentCoroutine()->Yield();
        // Run tests
        verify(frames_.size() == 5); // Verify we have all replicas
        auto testconfig = new RaftTestConfig(frames_);
        RaftLabTest test(testconfig);
        test.Run();
        test.Cleanup();
        // Turn off Reactor loop
        Reactor::GetReactor()->looping_ = false;
        return;
      });
      Log_info("raft_test_coro_ id=%d", raft_test_coro_->id);
    }
    
    // Wait for all communicators to be set
    bool all_commos_ready = false;
    while (!all_commos_ready) {
      all_commos_ready = true;
      for (const auto& pair : frames_) {
        if (pair.second->commo_ == nullptr) {
          all_commos_ready = false;
          break;
        }
      }
      if (!all_commos_ready) {
        raft_test_mutex_.unlock();
        sleep(0.1);
        raft_test_mutex_.lock();
      }
    }
  }
  
  raft_test_mutex_.unlock();
  #endif

  return commo_;
}

vector<rrr::Service *>
RaftFrame::CreateRpcServices(uint32_t site_id,
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

} // namespace janus;
