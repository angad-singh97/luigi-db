/**
 * @file server_main.cc
 * @brief RRR-based server entry point for Luigi protocol
 */

#include <iostream>
#include <memory>
#include <signal.h>
#include <string>

#include "benchmarks/benchmark_config.h"
#include "deptran/luigi/server.h"
#include "lib/common.h"

using namespace std;
using namespace mako;
using namespace janus;

// Global variables
static unique_ptr<LuigiServer> g_server;
static volatile bool g_running = true;

static void signal_handler(int sig) {
  cout << "\nReceived signal " << sig << ", shutting down...\n";
  g_running = false;
}

static void print_usage(const char *prog) {
  cerr << "Usage: " << prog << " [options]\n"
       << "\nRequired:\n"
       << "  --shard-config FILE    Configuration file\n"
       << "  --site-name NAME       Site name from config\n"
       << "\nOptional:\n"
       << "  --num-threads N        Number of worker threads (default: 4)\n"
       << "  --help                 Show this help\n";
}

int main(int argc, char **argv) {
  // Setup signal handlers
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  // Get BenchmarkConfig singleton
  auto &cfg = BenchmarkConfig::getInstance();

  // Parse command line arguments
  string site_name;
  string config_file;

  for (int i = 1; i < argc; i++) {
    string arg = argv[i];
    if (arg == "--shard-config" && i + 1 < argc) {
      config_file = argv[++i];
    } else if (arg == "--site-name" && i + 1 < argc) {
      site_name = argv[++i];
    } else if (arg == "--num-threads" && i + 1 < argc) {
      cfg.setNthreads(atoi(argv[++i]));
    } else if (arg == "--help") {
      print_usage(argv[0]);
      return 0;
    }
  }

  // Validate required arguments
  if (config_file.empty() || site_name.empty()) {
    cerr << "Error: --shard-config and --site-name are required\n";
    print_usage(argv[0]);
    return 1;
  }

  // Load configuration
  auto config = new transport::Configuration(config_file);
  cfg.setConfig(config);

  // Get site info
  auto site = config->GetSiteByName(site_name);
  if (!site) {
    cerr << "Error: Site '" << site_name << "' not found in config\n";
    return 1;
  }

  int shard_idx = site->shard_id;
  cfg.setShardIndex(shard_idx);
  cfg.setNshards(config->nshards);

  cout << "=== Luigi RRR Server Configuration ===\n"
       << "Site:       " << site_name << "\n"
       << "Shard:      " << shard_idx << "/" << cfg.getNshards() << "\n"
       << "Address:    " << site->ip << ":" << site->port << "\n"
       << "Threads:    " << cfg.getNthreads() << "\n"
       << "=======================================\n\n";

  // Create Luigi server
  cout << "Creating Luigi server...\n";
  g_server = make_unique<LuigiServer>(config_file, shard_idx);

  // Initialize and start
  cout << "Initializing server components...\n";
  g_server->Initialize();

  string bind_addr = site->ip + ":" + to_string(site->port);
  cout << "Starting RRR server on " << bind_addr << "...\n";
  g_server->Start(bind_addr);

  cout << "\n=== Luigi Server Ready ===\n";
  cout << "Listening for RPC requests...\n";
  cout << "Press Ctrl+C to stop\n\n";

  // Event loop
  while (g_running) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  // Cleanup
  cout << "\nShutting down...\n";
  g_server->Stop();
  g_server.reset();

  cout << "Luigi server stopped\n";
  return 0;
}
