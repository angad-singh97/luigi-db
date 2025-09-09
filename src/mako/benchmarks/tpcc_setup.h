// TPCC setup helpers extracted from tpcc.cc
#ifndef MAKO_BENCHMARKS_TPCC_SETUP_H
#define MAKO_BENCHMARKS_TPCC_SETUP_H

#include <thread>
#include <vector>
#include <unordered_map>
#include <map>
#include <atomic>
#include <string>

#include "bench.h"
#include <unordered_map>

// Decoupled setup helpers for TPCC benchmark: eRPC server and helper threads.
// These functions mirror the original logic in tpcc.cc but are now reusable.

namespace mako_tpcc_setup {

// Launch helper threads for all remote warehouses across shards.
// - Populates `helper_threads` with running threads.
// - Uses `queue_holders` and `queue_holders_response` for request/response queues.
void setup_helper(
  std::vector<std::thread> &helper_threads,
  abstract_db *db,
  const std::map<std::string, abstract_ordered_index *> &open_tables,
  const std::map<std::string, std::vector<abstract_ordered_index *>> &partitions,
  const std::map<std::string, std::vector<abstract_ordered_index *>> &dummy_partitions);

// Launch eRPC server threads and wire up per-warehouse queues.
// - Populates `server_transports` and fills `queue_holders{,_response}`.
// - Waits until all servers start by polling `set_server_transport`.
void setup_erpc_server();

// Stop all eRPC servers previously started by setup_erpc_server().
void stop_erpc_server();

} // namespace mako_tpcc_setup

#endif // MAKO_BENCHMARKS_TPCC_SETUP_H
