/**
 * @file luigi_server_main.cc
 * @brief Server-side entry point for Luigi protocol
 *
 * This is the Luigi equivalent of dbtest.cc - it creates LuigiServer instances
 * with scheduler/executor/OWD modules and runs the event loop.
 *
 * Reuses Mako's BenchmarkConfig for configuration management.
 *
 * Usage:
 *   ./luigi_server --shard-config config.yml --site-name site0 --num-threads 4
 */

#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#include "benchmarks/benchmark_config.h"
#include "benchmarks/rpc_setup.h"
#include "deptran/luigi/luigi_owd.h"
#include "deptran/luigi/luigi_server.h"
#include "deptran/luigi/luigi_state_machine.h"
#include "lib/common.h"

using namespace std;
using namespace mako;
using namespace janus;

static void print_usage(const char *prog) {
  cerr
      << "Usage: " << prog << " [options]\n"
      << "\nRequired:\n"
      << "  --shard-config FILE    Configuration file\n"
      << "  --site-name NAME       Site name from config\n"
      << "\nOptional:\n"
      << "  --num-threads N        Number of worker threads (default: 4)\n"
      << "  --benchmark TYPE       Benchmark type: tpcc|micro (default: tpcc)\n"
      << "  --duration SEC         Run duration in seconds (default: 30)\n"
      << "  --help                 Show this help\n";
}

int main(int argc, char **argv) {
  // Get BenchmarkConfig singleton
  auto &cfg = BenchmarkConfig::getInstance();

  // Parse command line arguments (reuse Mako's pattern)
  string site_name;
  string benchmark_type = "tpcc";

  for (int i = 1; i < argc; i++) {
    string arg = argv[i];
    if (arg == "--shard-config" && i + 1 < argc) {
      cfg.setConfigFile(argv[++i]);
    } else if (arg == "--site-name" && i + 1 < argc) {
      site_name = argv[++i];
    } else if (arg == "--num-threads" && i + 1 < argc) {
      cfg.setNthreads(atoi(argv[++i]));
    } else if (arg == "--benchmark" && i + 1 < argc) {
      benchmark_type = argv[++i];
    } else if (arg == "--duration" && i + 1 < argc) {
      cfg.setRuntime(atoi(argv[++i]));
    } else if (arg == "--help") {
      print_usage(argv[0]);
      return 0;
    }
  }

  // Validate required arguments
  if (cfg.getConfigFile().empty() || site_name.empty()) {
    cerr << "Error: --shard-config and --site-name are required\n";
    print_usage(argv[0]);
    return 1;
  }

  // Load configuration
  cfg.setConfig(new transport::Configuration(cfg.getConfigFile()));

  // Get site info and set shard index
  auto site = cfg.getConfig()->GetSiteByName(site_name);
  if (!site) {
    cerr << "Error: Site '" << site_name << "' not found in config\n";
    return 1;
  }

  cfg.setShardIndex(site->shard_id);
  cfg.setNshards(cfg.getConfig()->nshards);

  // Set cluster role
  if (site->is_leader) {
    cfg.setPaxosProcName(LOCALHOST_CENTER);
  } else if (site->replica_idx == 1) {
    cfg.setPaxosProcName(P1_CENTER);
  } else if (site->replica_idx == 2) {
    cfg.setPaxosProcName(P2_CENTER);
  } else {
    cfg.setPaxosProcName(LEARNER_CENTER);
  }

  cout << "=== Luigi Server Configuration ===\n"
       << "Site:       " << site_name << "\n"
       << "Shard:      " << cfg.getShardIndex() << "/" << cfg.getNshards()
       << "\n"
       << "Cluster:    " << cfg.getCluster() << "\n"
       << "Benchmark:  " << benchmark_type << "\n"
       << "Threads:    " << cfg.getNthreads() << "\n"
       << "===================================\n\n";

  // Initialize Luigi OWD service
  cout << "Initializing Luigi OWD service...\n";
  auto &owd = mako::luigi::LuigiOWD::getInstance();
  owd.init(cfg.getConfigFile(), cfg.getCluster(), cfg.getShardIndex(),
           cfg.getNshards());
  owd.start();

  // Create database and tables (reuse Mako's pattern)
  cout << "Creating database and tables...\n";
  abstract_db *db = nullptr;
  map<int, abstract_ordered_index *> tables;

  // TODO: Initialize database based on benchmark type
  // For now, this is a placeholder - actual DB initialization
  // should follow Mako's pattern from tpcc.cc

  // Create LuigiServer instance
  cout << "Creating Luigi server...\n";
  int partition_id = 0; // For single-partition mode
  LuigiServer *server = new LuigiServer(cfg.getConfigFile(),
                                        cfg.getShardIndex(), // client_shard_idx
                                        cfg.getShardIndex(), // server_shard_idx
                                        partition_id);

  // Create helper queues (reuse Mako's pattern)
  mako::HelperQueue *queue = new mako::HelperQueue();
  mako::HelperQueue *queue_response = new mako::HelperQueue();

  // Register database and tables
  if (db && !tables.empty()) {
    server->Register(db, queue, queue_response, tables);
  }

  // Setup RPC connections to other shards
  cout << "Setting up RPC connections...\n";
  // TODO: Create RPC server and poll thread
  // TODO: Get shard addresses from config
  // TODO: Call server->SetupRpc(rpc_server, poll_thread, shard_addresses)

  // Create state machine based on benchmark type
  cout << "Creating state machine (" << benchmark_type << ")..\n";
  shared_ptr<LuigiStateMachine> state_machine;

  if (benchmark_type == "tpcc") {
    state_machine =
        make_shared<LuigiTPCCStateMachine>(cfg.getShardIndex(), // shard_id
                                           0,                   // replica_id
                                           cfg.getNshards(),    // shard_num
                                           1                    // replica_num
        );
  } else if (benchmark_type == "micro") {
    state_machine = make_shared<LuigiMicroStateMachine>(cfg.getShardIndex(), 0,
                                                        cfg.getNshards(), 1);
  } else {
    cerr << "Error: Unknown benchmark type '" << benchmark_type << "'\n";
    delete server;
    delete queue;
    delete queue_response;
    owd.stop();
    return 1;
  }

  // Initialize state machine tables
  state_machine->InitializeTables();

  // Get scheduler from server and set state machine
  auto *scheduler = server->GetScheduler();
  if (scheduler) {
    scheduler->SetStateMachine(state_machine);
    scheduler->EnableStateMachineMode(true);
    scheduler->SetWorkerCount(cfg.getNthreads());
  }

  cout << "\n=== Luigi Server Ready ===\n";
  cout << "Listening for requests...\n";
  cout << "Press Ctrl+C to stop\n\n";

  // Run server event loop
  server->Run();

  // Cleanup
  cout << "\nShutting down...\n";
  delete server;
  delete queue;
  delete queue_response;
  if (db)
    delete db;
  owd.stop();

  cout << "Luigi server stopped\n";
  return 0;
}
