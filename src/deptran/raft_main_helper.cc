#include "raft_main_helper.h"

#ifdef MAKO_USE_RAFT

#include <iostream>

std::vector<std::string> setup(int /*argc*/, char* /*argv*/[]) {
  std::cerr << "[RAFT_STUB] setup() not yet implemented.\n";
  return {};
}

int setup2(int /*action*/, int /*shardIndex*/) {
  std::cerr << "[RAFT_STUB] setup2() not yet implemented.\n";
  return 0;
}

std::map<std::string, std::string> getHosts(std::string /*proc*/) {
  std::cerr << "[RAFT_STUB] getHosts() not yet implemented.\n";
  return {};
}

int get_outstanding_logs(uint32_t /*par_id*/) {
  std::cerr << "[RAFT_STUB] get_outstanding_logs() not yet implemented.\n";
  return 0;
}

int shutdown_paxos() {
  std::cerr << "[RAFT_STUB] shutdown_paxos() not yet implemented.\n";
  return 0;
}

void microbench_paxos() {
  std::cerr << "[RAFT_STUB] microbench_paxos() not yet implemented.\n";
}

void register_for_follower(std::function<void(const char*, int)> /*cb*/,
                           uint32_t /*par_id*/) {
  std::cerr << "[RAFT_STUB] register_for_follower() not yet implemented.\n";
}

void register_for_follower_par_id(
    std::function<void(const char*&, int, int)> /*cb*/, uint32_t /*par_id*/) {
  std::cerr << "[RAFT_STUB] register_for_follower_par_id() not yet implemented.\n";
}

void register_for_follower_par_id_return(
    std::function<int(const char*&, int, int, int,
                      std::queue<std::tuple<int, int, int, int, const char*>>&)> /*cb*/,
    uint32_t /*par_id*/) {
  std::cerr << "[RAFT_STUB] register_for_follower_par_id_return() not yet implemented.\n";
}

void register_for_leader(std::function<void(const char*, int)> /*cb*/,
                         uint32_t /*par_id*/) {
  std::cerr << "[RAFT_STUB] register_for_leader() not yet implemented.\n";
}

void register_leader_election_callback(std::function<void(int)> /*cb*/) {
  std::cerr << "[RAFT_STUB] register_leader_election_callback() not yet implemented.\n";
}

void register_for_leader_par_id(
    std::function<void(const char*&, int, int)> /*cb*/, uint32_t /*par_id*/) {
  std::cerr << "[RAFT_STUB] register_for_leader_par_id() not yet implemented.\n";
}

void register_for_leader_par_id_return(
    std::function<int(const char*&, int, int, int,
                      std::queue<std::tuple<int, int, int, int, const char*>>&)> /*cb*/,
    uint32_t /*par_id*/) {
  std::cerr << "[RAFT_STUB] register_for_leader_par_id_return() not yet implemented.\n";
}

void submit(const char* /*log*/, int /*len*/, uint32_t /*par_id*/) {
  std::cerr << "[RAFT_STUB] submit() not yet implemented.\n";
}

void add_log(const char* /*log*/, int /*len*/, uint32_t /*par_id*/) {
  std::cerr << "[RAFT_STUB] add_log() not yet implemented.\n";
}

void add_log_without_queue(const char* /*log*/, int /*len*/, uint32_t /*par_id*/) {
  std::cerr << "[RAFT_STUB] add_log_without_queue() not yet implemented.\n";
}

void add_log_to_nc(const char* /*log*/, int /*len*/, uint32_t /*par_id*/,
                   int /*batch_size*/) {
  std::cerr << "[RAFT_STUB] add_log_to_nc() not yet implemented.\n";
}

void wait_for_submit(uint32_t /*par_id*/) {
  std::cerr << "[RAFT_STUB] wait_for_submit() not yet implemented.\n";
}

void microbench_paxos_queue() {
  std::cerr << "[RAFT_STUB] microbench_paxos_queue() not yet implemented.\n";
}

void pre_shutdown_step() {
  std::cerr << "[RAFT_STUB] pre_shutdown_step() not yet implemented.\n";
}

int get_epoch() {
  std::cerr << "[RAFT_STUB] get_epoch() not yet implemented.\n";
  return 0;
}

void set_epoch(int /*epoch*/) {
  std::cerr << "[RAFT_STUB] set_epoch() not yet implemented.\n";
}

void upgrade_p1_to_leader() {
  std::cerr << "[RAFT_STUB] upgrade_p1_to_leader() not yet implemented.\n";
}

void worker_info_stats(size_t /*worker_id*/) {
  std::cerr << "[RAFT_STUB] worker_info_stats() not yet implemented.\n";
}

void nc_setup_server(int /*port*/, std::string /*ip*/) {
  std::cerr << "[RAFT_STUB] nc_setup_server() not yet implemented.\n";
}

std::vector<std::vector<int>>* nc_get_new_order_requests(int /*id*/) {
  std::cerr << "[RAFT_STUB] nc_get_new_order_requests() not yet implemented.\n";
  return nullptr;
}

std::vector<std::vector<int>>* nc_get_payment_requests(int /*id*/) {
  std::cerr << "[RAFT_STUB] nc_get_payment_requests() not yet implemented.\n";
  return nullptr;
}

std::vector<std::vector<int>>* nc_get_delivery_requests(int /*id*/) {
  std::cerr << "[RAFT_STUB] nc_get_delivery_requests() not yet implemented.\n";
  return nullptr;
}

std::vector<std::vector<int>>* nc_get_order_status_requests(int /*id*/) {
  std::cerr << "[RAFT_STUB] nc_get_order_status_requests() not yet implemented.\n";
  return nullptr;
}

std::vector<std::vector<int>>* nc_get_stock_level_requests(int /*id*/) {
  std::cerr << "[RAFT_STUB] nc_get_stock_level_requests() not yet implemented.\n";
  return nullptr;
}

std::vector<std::vector<int>>* nc_get_read_requests(int /*id*/) {
  std::cerr << "[RAFT_STUB] nc_get_read_requests() not yet implemented.\n";
  return nullptr;
}

std::vector<std::vector<int>>* nc_get_rmw_requests(int /*id*/) {
  std::cerr << "[RAFT_STUB] nc_get_rmw_requests() not yet implemented.\n";
  return nullptr;
}

#endif  // MAKO_USE_RAFT

