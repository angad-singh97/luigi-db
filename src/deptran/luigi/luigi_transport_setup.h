#pragma once

#include <string>

namespace janus {
namespace luigi {

/**
 * Setup Luigi transport infrastructure by delegating to Mako's
 * setup_erpc_server().
 *
 * This function:
 * 1. Populates BenchmarkConfig with Luigi-specific parameters
 * 2. Calls mako::setup_erpc_server() to create transports and helper queues
 * 3. Returns success/failure
 *
 * @param config_file Path to YAML configuration file
 * @param cluster Cluster name (e.g., "localhost", "dc0")
 * @param shard_index This shard's index
 * @param num_shards Total number of shards
 * @param num_erpc_servers Number of eRPC server threads (for load balancing)
 * @param warehouses Warehouses per shard (for TPC-C)
 * @return true on success
 */
bool setup_luigi_transport(const std::string &config_file,
                           const std::string &cluster, int shard_index,
                           int num_shards, int num_erpc_servers,
                           int warehouses);

/**
 * Stop Luigi transport infrastructure by delegating to Mako's
 * stop_erpc_server().
 */
void stop_luigi_transport();

} // namespace luigi
} // namespace janus
