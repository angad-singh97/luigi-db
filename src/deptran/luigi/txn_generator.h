#pragma once

/**
 * TxnGenerator - Tiga-style stored-procedure transaction generator for Luigi
 *
 * Key difference from Mako's STO architecture:
 * - STO: client does read -> modify -> write (multiple round trips)
 * - Stored procedure: client sends complete operation set upfront
 *
 * The generator produces LuigiOp vectors that can be directly dispatched
 * through Luigi's LuigiDispatchFromRequest() interface.
 */

#include <functional>
#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "luigi_entry.h"

namespace janus {

/**
 * Transaction request for Luigi stored-procedure style execution.
 * Similar to Tiga's ClientRequest.
 */
struct LuigiTxnRequest {
  uint32_t client_id = 0;
  uint32_t req_id = 0;
  uint64_t txn_id = 0;
  uint32_t txn_type = 0;

  // Working set: key -> value (like Tiga's ws_)
  std::map<int32_t, std::string> working_set;

  // Target shards for this transaction
  std::set<uint32_t> target_shards;

  // Pre-built LuigiOps for direct dispatch
  std::vector<LuigiOp> ops;

  // Callback for completion
  std::function<void(int status, uint64_t commit_ts,
                     const std::vector<std::string> &results)>
      callback;

  // Helper to get transaction key (unique identifier)
  uint64_t TxnKey() const {
    return (static_cast<uint64_t>(client_id) << 32) | req_id;
  }
};

/**
 * Transaction types
 */
enum LuigiTxnType {
  LUIGI_TXN_MICRO = 1,
  LUIGI_TXN_TPCC_NEW_ORDER = 10,
  LUIGI_TXN_TPCC_PAYMENT = 20,
  LUIGI_TXN_TPCC_ORDER_STATUS = 30,
  LUIGI_TXN_TPCC_DELIVERY = 40,
  LUIGI_TXN_TPCC_STOCK_LEVEL = 50,
};

/**
 * Configuration for transaction generators
 */
struct TxnGeneratorConfig {
  uint32_t shard_num = 1;

  // Micro benchmark config
  uint32_t key_num = 10000;
  double read_ratio = 0.5;
  uint32_t ops_per_txn = 10;

  // TPC-C config
  uint32_t num_warehouses = 1;
  uint32_t num_districts_per_wh = 10;
  uint32_t num_customers_per_district = 3000;
  uint32_t num_items = 100000;

  // TPC-C transaction mix weights
  double new_order_weight = 0.45;
  double payment_weight = 0.43;
  double order_status_weight = 0.04;
  double delivery_weight = 0.04;
  double stock_level_weight = 0.04;
};

/**
 * Base class for Luigi transaction generators (Tiga-style interface).
 */
class LuigiTxnGenerator {
protected:
  TxnGeneratorConfig config_;
  std::mt19937 rand_gen_;

public:
  LuigiTxnGenerator(const TxnGeneratorConfig &config) : config_(config) {
    rand_gen_.seed(std::random_device{}());
  }

  virtual ~LuigiTxnGenerator() = default;

  virtual std::string RTTI() = 0;

  /**
   * Generate a transaction request (main interface).
   */
  virtual void GetTxnReq(LuigiTxnRequest *req, uint32_t req_id,
                         uint32_t client_id) = 0;

  /**
   * Check if transaction needs multi-shard dispatch.
   */
  virtual bool NeedDispatch(const LuigiTxnRequest &req) {
    return req.target_shards.size() > 1;
  }

  const TxnGeneratorConfig &config() const { return config_; }
  void Seed(uint64_t seed) { rand_gen_.seed(seed); }

protected:
  int32_t RandomInt(int32_t min, int32_t max) {
    if (min >= max)
      return min;
    std::uniform_int_distribution<int32_t> dist(min, max - 1);
    return dist(rand_gen_);
  }

  double RandomDouble() {
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(rand_gen_);
  }

  uint32_t KeyToShard(int32_t key) const {
    return static_cast<uint32_t>(key) % config_.shard_num;
  }

  static LuigiOp MakeOp(uint16_t table_id, uint8_t op_type,
                        const std::string &key, const std::string &value = "") {
    LuigiOp op;
    op.table_id = table_id;
    op.op_type = op_type;
    op.key = key;
    op.value = value;
    return op;
  }

  static std::string KeyToString(int32_t key) { return std::to_string(key); }

  static std::string ValueToString(int32_t value) {
    return std::to_string(value);
  }
};

} // namespace janus
