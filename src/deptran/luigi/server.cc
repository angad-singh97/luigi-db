#include "server.h"

#include "commo.h"
#include "executor.h"
#include "scheduler.h"
#include "service.h"
#include "state_machine.h"

#include "rrr.hpp"

namespace janus {

LuigiServer::LuigiServer(const std::string &config_file, int shard_idx)
    : config_(config_file), partition_id_(shard_idx), shard_idx_(shard_idx) {}

LuigiServer::~LuigiServer() {
  Stop();

  delete scheduler_;
  delete executor_;
  delete state_machine_;
}

void LuigiServer::Initialize() {
  // Create communicator
  commo_ = std::make_shared<LuigiCommo>(rusty::None);

  // Create scheduler
  scheduler_ = new SchedulerLuigi();
  scheduler_->partition_id_ = partition_id_;

  // Create executor
  executor_ = new LuigiExecutor();

  // Note: state_machine should be set externally via SetStateMachine()

  // Create RPC service
  service_ = std::make_unique<LuigiServiceImpl>(this);

  // Create RRR server
  rpc_server_ = std::make_unique<rrr::Server>();
  rpc_server_->reg(service_.get());
}

void LuigiServer::Start(const std::string &bind_addr) {
  if (!rpc_server_) {
    Initialize();
  }

  rpc_server_->start(bind_addr.c_str());
  Log_info("Luigi server started on %s", bind_addr.c_str());
}

void LuigiServer::Stop() {
  if (rpc_server_) {
    rpc_server_.reset();
  }
}

} // namespace janus
