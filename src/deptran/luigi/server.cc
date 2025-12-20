/**
 * @file server.cc
 * @brief LuigiServer implementation and main entry point
 *
 * Usage: ./luigi_server -f config.yml -P process_name
 *
 * Server role (leader/follower) is determined from config file based on
 * site position in the shard list (first = leader, rest = followers).
 */

#include "server.h"

#include "commo.h"
#include "executor.h"
#include "scheduler.h"
#include "service.h"
#include "state_machine.h"

#include "../__dep__.h"
#include "../config.h"
#include "rrr.hpp"

#include <iostream>
#include <memory>
#include <signal.h>
#include <string>
#include <thread>

namespace janus {

//=============================================================================
// LuigiServer Implementation
//=============================================================================

LuigiServer::LuigiServer(int partition_id)
    : partition_id_(partition_id), shard_idx_(partition_id) {}

LuigiServer::~LuigiServer() {
  Stop();
  delete scheduler_;
}

void LuigiServer::Initialize(LuigiRole role, siteid_t site_id) {
  // Create role-aware communicator
  commo_ =
      std::make_shared<LuigiCommo>(role, partition_id_, site_id, rusty::None);

  // Create scheduler and set partition
  scheduler_ = new SchedulerLuigi();
  scheduler_->partition_id_ = partition_id_;
  scheduler_->SetPartitionId(partition_id_);
  scheduler_->commo_ = commo_.get();

  // Create RPC service
  service_ = std::make_unique<LuigiServiceImpl>(this);

  Log_info("LuigiServer initialized: shard=%d, site=%d, role=%s", partition_id_,
           site_id, role == LuigiRole::LEADER ? "LEADER" : "FOLLOWER");
}

void LuigiServer::Start(const std::string &bind_addr) {
  if (!service_) {
    Log_error("LuigiServer::Start called before Initialize");
    return;
  }

  // Create and start RRR server
  auto poll = PollThread::create();
  base::ThreadPool *thread_pool = new base::ThreadPool(1);
  rpc_server_ = std::make_unique<rrr::Server>(poll, thread_pool);
  rpc_server_->reg(service_.get());
  rpc_server_->start(bind_addr.c_str());

  // Start scheduler threads
  scheduler_->Start();

  Log_info("Luigi server started on %s for shard %d", bind_addr.c_str(),
           partition_id_);
}

void LuigiServer::Stop() {
  if (scheduler_) {
    scheduler_->Stop();
  }
  if (rpc_server_) {
    rpc_server_.reset();
  }
}

} // namespace janus

//=============================================================================
// Main Entry Point
//=============================================================================

using namespace std;
using namespace janus;

static vector<unique_ptr<LuigiServer>> g_servers;
static volatile bool g_running = true;

static void signal_handler(int sig) {
  cout << "\nReceived signal " << sig << ", shutting down...\n";
  g_running = false;
}

static void print_usage(const char *prog) {
  cerr << "Usage: " << prog << " -f config.yml -P process_name\n"
       << "\nRequired:\n"
       << "  -f FILE        Configuration file (YAML)\n"
       << "  -P NAME        Process name (matches 'process:' in config)\n"
       << "\nOptional:\n"
       << "  -h             Show this help\n"
       << "\nExample:\n"
       << "  " << prog << " -f config/local-2shard.yml -P s0\n";
}

int main(int argc, char **argv) {
  // Setup signal handlers
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  // Use janus::Config to parse arguments
  int ret = Config::CreateConfig(argc, argv);
  if (ret != 0) {
    cerr
        << "Error: Failed to parse config. Use -f config.yml -P process_name\n";
    print_usage(argv[0]);
    return 1;
  }

  auto config = Config::GetConfig();
  auto my_sites = config->GetMyServers();

  if (my_sites.empty()) {
    cerr << "Error: No server sites found for process '" << config->proc_name_
         << "'\n";
    cerr << "Check that -P matches a 'process:' entry in config\n";
    return 1;
  }

  cout << "=== Luigi Server ===\n"
       << "Process:    " << config->proc_name_ << "\n"
       << "Sites:      " << my_sites.size() << "\n";

  // Create and start a server for each site this process owns
  for (auto &site : my_sites) {
    // Determine role from site.role (0 = leader, 1+ = follower)
    LuigiRole role = (site.role == 0) ? LuigiRole::LEADER : LuigiRole::FOLLOWER;

    cout << "  - Site " << site.id << ": shard=" << site.partition_id_
         << " role=" << (role == LuigiRole::LEADER ? "LEADER" : "FOLLOWER")
         << " addr=" << site.GetBindAddress() << "\n";

    auto server = make_unique<LuigiServer>(site.partition_id_);

    // Create and set state machine
    int num_shards = config->GetNumPartition();
    auto state_machine = make_shared<LuigiMicroStateMachine>(site.partition_id_,
                                                             0, num_shards, 1);
    state_machine->InitializeTables();
    server->SetStateMachine(state_machine);

    // Initialize with role and site info
    server->Initialize(role, site.id);
    server->Start(site.GetBindAddress());

    g_servers.push_back(std::move(server));
  }

  cout << "\n=== Luigi Server Ready ===\n"
       << "Press Ctrl+C to stop\n\n";

  // Event loop
  while (g_running) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  // Cleanup
  cout << "\nShutting down " << g_servers.size() << " server(s)...\n";
  for (auto &server : g_servers) {
    server->Stop();
  }
  g_servers.clear();

  cout << "Luigi server stopped\n";
  return 0;
}
