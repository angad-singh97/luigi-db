/**
 * @file luigi_bench_main.cc
 * @brief Main entry point for Luigi stored-procedure benchmark
 *
 * This benchmark tests Luigi's timestamp-ordered execution protocol
 * using Tiga-style stored procedures (not STO-style wrappers).
 *
 * Compatible with Mako CI configuration files!
 *
 * Usage (Mako CI compatible):
 *   ./luigi_bench --shard-config <shard_config.yml> --shard-index <idx> \
 *                 -P <cluster> --num-threads <n> --duration <sec>
 *
 * Or standalone:
 *   ./luigi_bench --config <shard_config> --shard <shard_idx> \
 *                 --cluster <cluster_name> --benchmark <micro|tpcc> \
 *                 --threads <num_threads> --duration <seconds>
 *
 * Examples:
 *   # Mako CI style (uses same args as dbtest)
 *   ./luigi_bench --shard-config src/mako/config/local-shards2-warehouses6.yml
 * \
 *                 --shard-index 0 -P localhost --num-threads 6 --duration 30
 *
 *   # Run micro benchmark for 30 seconds with 4 threads
 *   ./luigi_bench --config config/shards.yml --shard 0 --cluster dc0 \
 *                 --benchmark micro --threads 4 --duration 30
 *
 *   # Run TPC-C benchmark
 *   ./luigi_bench --config config/shards.yml --shard 0 --cluster dc0 \
 *                 --benchmark tpcc --threads 8 --duration 60
 */

#include <cstdlib>
#include <getopt.h>
#include <iostream>
#include <string>

#include "deptran/luigi/benchmark_client.h"
#include "deptran/luigi/owd.h"
#include "deptran/luigi/luigi_transport_setup.h"
#include "mako/lib/configuration.h"

using namespace mako::luigi;
using namespace janus::luigi; // For transport setup functions

void PrintUsage(const char *prog) {
  std::cerr
      << "Usage: " << prog << " [options]\n"
      << "\nMako CI Compatible Options (same as dbtest):\n"
      << "  -q, --shard-config <file>  Shard configuration file (required)\n"
      << "  -g, --shard-index <idx>    Shard index (default: 0)\n"
      << "  -P <cluster>               Cluster/process name (localhost, p1, "
         "p2, learner)\n"
      << "  -t, --num-threads <n>      Number of worker threads (default: 1)\n"
      << "\nStandalone Options:\n"
      << "  -c, --config <file>        Alias for --shard-config\n"
      << "  -C, --cluster <name>       Alias for -P (default: localhost)\n"
      << "  -b, --benchmark <type>     Benchmark type: micro, micro_single, "
         "tpcc (default: tpcc)\n"
      << "  -d, --duration <sec>       Benchmark duration in seconds (default: "
         "30)\n"
      << "  -k, --keys <n>             Number of keys per shard for micro "
         "(default: 100000)\n"
      << "  -r, --read-ratio <r>       Read ratio for micro benchmark "
         "(default: 0.5)\n"
      << "  -o, --ops <n>              Operations per transaction for micro "
         "(default: 10)\n"
      << "  -h, --help                 Show this help message\n"
      << "\nThe benchmark reads warehouses count from the YAML config file.\n";
}

int main(int argc, char *argv[]) {
  // Default configuration values (can be overridden by CLI)
  std::string config_file = "";
  std::string cluster = "localhost"; // Default to localhost like Mako CI
  uint32_t shard_idx = 0;
  uint32_t par_id = 0;     // Not exposed via CLI, always 0 for now
  uint32_t num_shards = 0; // 0 means read from config file
  uint32_t num_threads = 1;
  uint32_t duration_sec = 30; // Default 30s like typical CI tests

  std::string benchmark_type = "tpcc"; // Default to TPC-C like Mako CI
  int keys_per_shard = 100000;
  int warehouses = 0; // 0 means read from config file
  double read_ratio = 0.5;
  int ops_per_txn = 10;

  // Parse command line arguments - support both Mako CI style and standalone
  static struct option long_options[] = {
      // Mako CI compatible options
      {"shard-config", required_argument, 0, 'q'}, // -q like dbtest
      {"shard-index", required_argument, 0, 'g'},  // -g like dbtest
      {"num-threads", required_argument, 0, 't'},  // -t like dbtest
      // Standalone options
      {"config", required_argument, 0, 'c'}, // Alias for -q
      {"shard", required_argument, 0,
       'G'}, // Alias for -g (uppercase to avoid conflict)
      {"cluster", required_argument, 0, 'C'},
      {"benchmark", required_argument, 0, 'b'},
      {"threads", required_argument, 0, 'T'}, // Alias for -t
      {"duration", required_argument, 0, 'd'},
      {"keys", required_argument, 0, 'k'},
      {"warehouses", required_argument, 0, 'w'},
      {"read-ratio", required_argument, 0, 'r'},
      {"ops", required_argument, 0, 'o'},
      {"help", no_argument, 0, 'h'},
      {0, 0, 0, 0}};

  int opt;
  int option_index = 0;
  while ((opt = getopt_long(argc, argv, "q:g:t:c:G:C:b:T:d:k:w:r:o:P:h",
                            long_options, &option_index)) != -1) {
    switch (opt) {
    case 'q': // --shard-config (Mako CI style)
    case 'c': // --config (standalone alias)
      config_file = optarg;
      break;
    case 'g': // --shard-index (Mako CI style)
    case 'G': // --shard (standalone alias)
      shard_idx = std::atoi(optarg);
      break;
    case 'P': // -P cluster (Mako CI style, like dbtest)
    case 'C': // --cluster (standalone alias)
      cluster = optarg;
      break;
    case 'b': // --benchmark
      benchmark_type = optarg;
      break;
    case 't': // --num-threads (Mako CI style)
    case 'T': // --threads (standalone alias)
      num_threads = std::atoi(optarg);
      break;
    case 'd': // --duration
      duration_sec = std::atoi(optarg);
      break;
    case 'k': // --keys
      keys_per_shard = std::atoi(optarg);
      break;
    case 'w': // --warehouses
      warehouses = std::atoi(optarg);
      break;
    case 'r': // --read-ratio
      read_ratio = std::atof(optarg);
      break;
    case 'o': // --ops
      ops_per_txn = std::atoi(optarg);
      break;
    case 'h':
    default:
      PrintUsage(argv[0]);
      return opt == 'h' ? 0 : 1;
    }
  }

  // Validate required options
  if (config_file.empty()) {
    std::cerr << "Error: --shard-config or --config is required\n";
    PrintUsage(argv[0]);
    return 1;
  }

  // Parse the YAML config to extract shard count and warehouses
  try {
    transport::Configuration yaml_config(config_file);

    // Get number of shards from config if not specified
    if (num_shards == 0) {
      num_shards = yaml_config.nshards;
      std::cout << "Read num_shards=" << num_shards << " from config file\n";
    }

    // Get warehouses from config if not specified (for TPC-C)
    if (warehouses == 0 && yaml_config.warehouses > 0) {
      warehouses = yaml_config.warehouses;
      std::cout << "Read warehouses=" << warehouses << " from config file\n";
    }
  } catch (const std::exception &e) {
    std::cerr << "Warning: Could not parse YAML config: " << e.what() << "\n";
    std::cerr << "Using command-line defaults.\n";
  }

  // Fall back to defaults if still not set
  if (num_shards == 0)
    num_shards = 1;
  if (warehouses == 0)
    warehouses = num_threads; // Common Mako pattern

  // Create benchmark client config
  LuigiBenchmarkClient::Config client_config;

  // Setup generator config based on benchmark type
  if (benchmark_type == "micro" || benchmark_type == "micro_single") {
    client_config.gen_config =
        CreateDefaultMicroConfig(num_shards, keys_per_shard);
    client_config.gen_config.read_ratio = read_ratio;
    client_config.gen_config.ops_per_txn = ops_per_txn;
  } else if (benchmark_type == "tpcc") {
    client_config.gen_config = CreateDefaultTPCCConfig(num_shards, warehouses);
  } else {
    std::cerr << "Error: Unknown benchmark type '" << benchmark_type << "'\n";
    std::cerr << "Valid types: micro, micro_single, tpcc\n";
    return 1;
  }

  // Initialize Luigi OWD service (for calculating expected timestamps)
  std::cout << "Initializing Luigi OWD service..." << std::endl;
  auto &luigiOwd = LuigiOWD::getInstance();
  luigiOwd.init(config_file, cluster, shard_idx, num_shards);
  luigiOwd.start();

  // Setup transport infrastructure (delegates to Mako's setup_erpc_server)
  std::cout << "Setting up transport infrastructure..." << std::endl;
  int num_erpc_servers = num_threads; // One eRPC server per thread
  if (!luigi::setup_luigi_transport(config_file, cluster, shard_idx, num_shards,
                                    num_erpc_servers, warehouses)) {
    std::cerr << "Failed to setup transport infrastructure" << std::endl;
    luigiOwd.stop();
    return 1;
  }

  // Create and initialize benchmark client
  std::cout << "Creating benchmark client..." << std::endl;

  // Configure worker IDs for per-worker replication
  // Default to 0 for single-VM setup
  uint32_t client_vm_index = 0;
  uint32_t num_workers_per_vm = num_threads;

  client_config.worker_id_base = client_vm_index * num_workers_per_vm;
  client_config.num_workers_per_vm = num_workers_per_vm;
  client_config.config_file = config_file; // Set config file path

  Log_info("Luigi worker ID configuration: client_vm_index=%u, "
           "num_workers_per_vm=%u, worker_id_base=%u",
           client_vm_index, num_workers_per_vm, client_config.worker_id_base);

  LuigiBenchmarkClient client(client_config);
  if (!client.Initialize()) {
    std::cerr << "Failed to initialize benchmark client" << std::endl;
    luigi::stop_luigi_transport();
    luigiOwd.stop();
    return 1;
  }

  // Print configuration
  std::cout << "\n========== Luigi Benchmark Configuration ==========\n";
  std::cout << "Config file:    " << config_file << "\n";
  std::cout << "Cluster:        " << cluster << "\n";
  std::cout << "Shard index:    " << shard_idx << "\n";
  std::cout << "Num shards:     " << num_shards << "\n";
  std::cout << "Threads:        " << num_threads << "\n";
  std::cout << "Duration:       " << duration_sec << "s\n";
  std::cout << "Benchmark:      " << benchmark_type << "\n";
  if (benchmark_type == "micro" || benchmark_type == "micro_single") {
    std::cout << "Keys/shard:     " << keys_per_shard << "\n";
    std::cout << "Read ratio:     " << read_ratio << "\n";
    std::cout << "Ops/txn:        " << ops_per_txn << "\n";
  } else {
    std::cout << "Warehouses:     " << warehouses << "\n";
  }
  std::cout << "====================================================\n\n";

  // Run benchmark
  BenchmarkStats stats;
  if (benchmark_type == "micro") {
    stats = client.RunMicroBenchmark();
  } else if (benchmark_type == "micro_single") {
    stats = client.RunSingleShardMicroBenchmark();
  } else if (benchmark_type == "tpcc") {
    stats = client.RunTPCCBenchmark();
  }

  // Print results
  stats.Print();

  // Cleanup
  luigi::stop_luigi_transport();
  // Cleanup OWD module
  luigiOwd.stop();
  std::cout << "Luigi OWD module stopped\n";

  return 0;
}
