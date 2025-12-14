#include "luigi_rpc_setup.h"
#include "luigi_scheduler.h"

#include "deptran/__dep__.h" // For logging
#include "deptran/rcc/tx.h"  // For parent_set_t definition
#include "deptran/rcc_rpc.h" // For LuigiLeaderProxy

#include "rrr/rrr.hpp"

namespace janus {

LuigiRpcSetup::LuigiRpcSetup() {}

LuigiRpcSetup::~LuigiRpcSetup() { Shutdown(); }

bool LuigiRpcSetup::SetupService(rrr::Server *rpc_server,
                                 SchedulerLuigi *scheduler) {
  if (rpc_server == nullptr || scheduler == nullptr) {
    Log_error("LuigiRpcSetup::SetupService: null server or scheduler");
    return false;
  }

  if (service_ != nullptr) {
    Log_warn("LuigiRpcSetup::SetupService: service already set up");
    return true;
  }

  // Create the service implementation
  // RRR service removed - now using eRPC for all coordination
  Log_info("LuigiRpcSetup: Using eRPC for leader coordination");

  // RRR service registration code removed as per instruction.
  // The service_ member is now expected to be managed externally or not used
  // for RRR. If eRPC is used, its setup would be elsewhere.

  // Assuming that if RRR service is removed, this function should just return
  // true if it reaches this point, indicating no RRR service setup is needed.
  return true;
}

int LuigiRpcSetup::ConnectToLeaders(
    const std::map<uint32_t, std::string> &shard_addresses,
    rusty::Arc<rrr::PollThread> poll_thread, SchedulerLuigi *scheduler) {

  if (!poll_thread || scheduler == nullptr) {
    Log_error("LuigiRpcSetup::ConnectToLeaders: null poll_thread or scheduler");
    return 0;
  }

  int connected = 0;

  for (const auto &[shard_id, addr] : shard_addresses) {
    Log_info("Luigi: connecting to shard %u at %s", shard_id, addr.c_str());

    // Create RPC client using the factory method
    auto client = rrr::Client::create(poll_thread);
    int ret = client->connect(addr.c_str());

    if (ret != 0) {
      Log_warn("Luigi: failed to connect to shard %u at %s (ret=%d)", shard_id,
               addr.c_str(), ret);
      client->close();
      continue;
    }

    // Create proxy - need to get raw pointer for proxy constructor
    auto *proxy = new LuigiLeaderProxy(const_cast<rrr::Client *>(client.get()));

    // Add to scheduler
    scheduler->AddLeaderProxy(shard_id, proxy);

    // Track for cleanup
    rpc_clients_.push_back(client);
    proxies_.push_back(proxy);

    Log_info("Luigi: connected to shard %u", shard_id);
    connected++;
  }

  Log_info("Luigi: connected to %d/%zu leader shards", connected,
           shard_addresses.size());

  return connected;
}

void LuigiRpcSetup::Shutdown() {
  // Clean up proxies (they don't own the clients)
  for (auto *proxy : proxies_) {
    delete proxy;
  }
  // RRR service removed - using eRPC for all coordination
  Log_info("Luigi RPC shutdown complete");
}

} // namespace janus
