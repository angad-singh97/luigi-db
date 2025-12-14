#include "luigi_state_machine.h"
#include "__dep__.h"

#include <chrono>
#include <random>

namespace janus {

//=============================================================================
// LuigiMicroStateMachine Implementation
//=============================================================================

bool LuigiMicroStateMachine::Execute(uint32_t txn_type,
                                      const std::vector<LuigiOp>& ops,
                                      std::map<std::string, std::string>* output,
                                      uint64_t txn_id) {
    // Simple execution: process each op sequentially
    for (const auto& op : ops) {
        if (!IsLocalOp(op)) {
            // Skip ops for other shards (should be routed there)
            continue;
        }

        if (op.op_type == LUIGI_OP_READ) {
            std::string value;
            if (Get(op.key, value)) {
                if (output) {
                    (*output)[op.key] = value;
                }
            } else {
                // Key not found - return empty string
                if (output) {
                    (*output)[op.key] = "";
                }
            }
        } else if (op.op_type == LUIGI_OP_WRITE) {
            Put(op.key, op.value);
        }
    }
    return true;
}

void LuigiMicroStateMachine::PopulateData() {
    // Populate with some initial data for micro benchmarks
    // Key format: "key_<shard>_<index>"
    
    const uint32_t keys_per_shard = 1000000;  // 1M keys per shard
    
    std::default_random_engine rng(shard_id_ * 12345);
    std::uniform_int_distribution<> val_dist(0, 999999);

    for (uint32_t i = 0; i < keys_per_shard; i++) {
        std::string key = "key_" + std::to_string(shard_id_) + "_" + std::to_string(i);
        std::string value = std::to_string(val_dist(rng));
        Put(key, value);
    }
    
    Log_info("LuigiMicroStateMachine[%u]: Populated %zu keys", 
             shard_id_, kv_store_.size());
}

//=============================================================================
// LuigiTPCCStateMachine Implementation
//=============================================================================

LuigiTPCCStateMachine::~LuigiTPCCStateMachine() {
    // Clean up schemas
    for (auto* schema : schemas_) {
        delete schema;
    }
    schemas_.clear();
    
    // TxnMgr owns tables, no need to delete them
}

void LuigiTPCCStateMachine::InitializeTables() {
    //=========================================================================
    // Create TPC-C Schemas (following Tiga's TPCCStateMachine)
    //=========================================================================
    
    // Warehouse table: W_ID (i32 key), W_NAME, W_YTD (double)
    {
        auto* schema = new mdb::Schema();
        schema->add_column("w_id", mdb::Value::I32, true);  // primary key
        schema->add_column("w_name", mdb::Value::STR);
        schema->add_column("w_street_1", mdb::Value::STR);
        schema->add_column("w_street_2", mdb::Value::STR);
        schema->add_column("w_city", mdb::Value::STR);
        schema->add_column("w_state", mdb::Value::STR);
        schema->add_column("w_zip", mdb::Value::STR);
        schema->add_column("w_tax", mdb::Value::DOUBLE);
        schema->add_column("w_ytd", mdb::Value::DOUBLE);
        schemas_.push_back(schema);
        tbl_warehouse_ = new mdb::SortedTable(schema);
        txn_mgr_.reg_table("warehouse", tbl_warehouse_);
    }

    // District table: D_ID + D_W_ID (composite key)
    {
        auto* schema = new mdb::Schema();
        schema->add_column("d_id", mdb::Value::I32, true);
        schema->add_column("d_w_id", mdb::Value::I32, true);
        schema->add_column("d_name", mdb::Value::STR);
        schema->add_column("d_street_1", mdb::Value::STR);
        schema->add_column("d_street_2", mdb::Value::STR);
        schema->add_column("d_city", mdb::Value::STR);
        schema->add_column("d_state", mdb::Value::STR);
        schema->add_column("d_zip", mdb::Value::STR);
        schema->add_column("d_tax", mdb::Value::DOUBLE);
        schema->add_column("d_ytd", mdb::Value::DOUBLE);
        schema->add_column("d_next_o_id", mdb::Value::I32);
        schemas_.push_back(schema);
        tbl_district_ = new mdb::SortedTable(schema);
        txn_mgr_.reg_table("district", tbl_district_);
    }

    // Customer table: C_ID + C_D_ID + C_W_ID (composite key)
    {
        auto* schema = new mdb::Schema();
        schema->add_column("c_id", mdb::Value::I32, true);
        schema->add_column("c_d_id", mdb::Value::I32, true);
        schema->add_column("c_w_id", mdb::Value::I32, true);
        schema->add_column("c_first", mdb::Value::STR);
        schema->add_column("c_middle", mdb::Value::STR);
        schema->add_column("c_last", mdb::Value::STR);
        schema->add_column("c_street_1", mdb::Value::STR);
        schema->add_column("c_street_2", mdb::Value::STR);
        schema->add_column("c_city", mdb::Value::STR);
        schema->add_column("c_state", mdb::Value::STR);
        schema->add_column("c_zip", mdb::Value::STR);
        schema->add_column("c_phone", mdb::Value::STR);
        schema->add_column("c_since", mdb::Value::STR);
        schema->add_column("c_credit", mdb::Value::STR);
        schema->add_column("c_credit_lim", mdb::Value::DOUBLE);
        schema->add_column("c_discount", mdb::Value::DOUBLE);
        schema->add_column("c_balance", mdb::Value::DOUBLE);
        schema->add_column("c_ytd_payment", mdb::Value::DOUBLE);
        schema->add_column("c_payment_cnt", mdb::Value::I32);
        schema->add_column("c_delivery_cnt", mdb::Value::I32);
        schema->add_column("c_data", mdb::Value::STR);
        schemas_.push_back(schema);
        tbl_customer_ = new mdb::SortedTable(schema);
        txn_mgr_.reg_table("customer", tbl_customer_);
    }

    // History table (no primary key in TPC-C spec, use row_id)
    {
        auto* schema = new mdb::Schema();
        schema->add_column("h_row_id", mdb::Value::I64, true);
        schema->add_column("h_c_id", mdb::Value::I32);
        schema->add_column("h_c_d_id", mdb::Value::I32);
        schema->add_column("h_c_w_id", mdb::Value::I32);
        schema->add_column("h_d_id", mdb::Value::I32);
        schema->add_column("h_w_id", mdb::Value::I32);
        schema->add_column("h_date", mdb::Value::STR);
        schema->add_column("h_amount", mdb::Value::DOUBLE);
        schema->add_column("h_data", mdb::Value::STR);
        schemas_.push_back(schema);
        tbl_history_ = new mdb::SortedTable(schema);
        txn_mgr_.reg_table("history", tbl_history_);
    }

    // NewOrder table: NO_O_ID + NO_D_ID + NO_W_ID
    {
        auto* schema = new mdb::Schema();
        schema->add_column("no_o_id", mdb::Value::I32, true);
        schema->add_column("no_d_id", mdb::Value::I32, true);
        schema->add_column("no_w_id", mdb::Value::I32, true);
        schemas_.push_back(schema);
        tbl_new_order_ = new mdb::SortedTable(schema);
        txn_mgr_.reg_table("new_order", tbl_new_order_);
    }

    // Order table: O_ID + O_D_ID + O_W_ID
    {
        auto* schema = new mdb::Schema();
        schema->add_column("o_id", mdb::Value::I32, true);
        schema->add_column("o_d_id", mdb::Value::I32, true);
        schema->add_column("o_w_id", mdb::Value::I32, true);
        schema->add_column("o_c_id", mdb::Value::I32);
        schema->add_column("o_entry_d", mdb::Value::STR);
        schema->add_column("o_carrier_id", mdb::Value::I32);
        schema->add_column("o_ol_cnt", mdb::Value::I32);
        schema->add_column("o_all_local", mdb::Value::I32);
        schemas_.push_back(schema);
        tbl_order_ = new mdb::SortedTable(schema);
        txn_mgr_.reg_table("order", tbl_order_);
    }

    // OrderLine table: OL_O_ID + OL_D_ID + OL_W_ID + OL_NUMBER
    {
        auto* schema = new mdb::Schema();
        schema->add_column("ol_o_id", mdb::Value::I32, true);
        schema->add_column("ol_d_id", mdb::Value::I32, true);
        schema->add_column("ol_w_id", mdb::Value::I32, true);
        schema->add_column("ol_number", mdb::Value::I32, true);
        schema->add_column("ol_i_id", mdb::Value::I32);
        schema->add_column("ol_supply_w_id", mdb::Value::I32);
        schema->add_column("ol_delivery_d", mdb::Value::STR);
        schema->add_column("ol_quantity", mdb::Value::I32);
        schema->add_column("ol_amount", mdb::Value::DOUBLE);
        schema->add_column("ol_dist_info", mdb::Value::STR);
        schemas_.push_back(schema);
        tbl_order_line_ = new mdb::SortedTable(schema);
        txn_mgr_.reg_table("order_line", tbl_order_line_);
    }

    // Item table: I_ID
    {
        auto* schema = new mdb::Schema();
        schema->add_column("i_id", mdb::Value::I32, true);
        schema->add_column("i_im_id", mdb::Value::I32);
        schema->add_column("i_name", mdb::Value::STR);
        schema->add_column("i_price", mdb::Value::DOUBLE);
        schema->add_column("i_data", mdb::Value::STR);
        schemas_.push_back(schema);
        tbl_item_ = new mdb::SortedTable(schema);
        txn_mgr_.reg_table("item", tbl_item_);
    }

    // Stock table: S_I_ID + S_W_ID
    {
        auto* schema = new mdb::Schema();
        schema->add_column("s_i_id", mdb::Value::I32, true);
        schema->add_column("s_w_id", mdb::Value::I32, true);
        schema->add_column("s_quantity", mdb::Value::I32);
        schema->add_column("s_dist_01", mdb::Value::STR);
        schema->add_column("s_dist_02", mdb::Value::STR);
        schema->add_column("s_dist_03", mdb::Value::STR);
        schema->add_column("s_dist_04", mdb::Value::STR);
        schema->add_column("s_dist_05", mdb::Value::STR);
        schema->add_column("s_dist_06", mdb::Value::STR);
        schema->add_column("s_dist_07", mdb::Value::STR);
        schema->add_column("s_dist_08", mdb::Value::STR);
        schema->add_column("s_dist_09", mdb::Value::STR);
        schema->add_column("s_dist_10", mdb::Value::STR);
        schema->add_column("s_ytd", mdb::Value::I32);
        schema->add_column("s_order_cnt", mdb::Value::I32);
        schema->add_column("s_remote_cnt", mdb::Value::I32);
        schema->add_column("s_data", mdb::Value::STR);
        schemas_.push_back(schema);
        tbl_stock_ = new mdb::SortedTable(schema);
        txn_mgr_.reg_table("stock", tbl_stock_);
    }

    Log_info("LuigiTPCCStateMachine[%u]: Tables initialized", shard_id_);
}

void LuigiTPCCStateMachine::PopulateData() {
    // Note: This is simplified data loading
    // Production code would need proper TPC-C data population
    
    std::default_random_engine rng(shard_id_ * 54321);
    
    // Populate warehouses (each shard owns some warehouses)
    auto* txn = txn_mgr_.start(0);
    
    for (uint32_t w = 0; w < num_warehouses_; w++) {
        if (KeyToShard("warehouse_" + std::to_string(w)) != shard_id_) {
            continue;  // This warehouse belongs to another shard
        }
        
        // Create warehouse row
        std::vector<mdb::Value> row_data = {
            mdb::Value((i32)w),                          // w_id
            mdb::Value("Warehouse_" + std::to_string(w)), // w_name
            mdb::Value("Street1"),                        // w_street_1
            mdb::Value("Street2"),                        // w_street_2
            mdb::Value("City"),                           // w_city
            mdb::Value("ST"),                             // w_state
            mdb::Value("12345"),                          // w_zip
            mdb::Value(0.0875),                           // w_tax
            mdb::Value(300000.0)                          // w_ytd
        };
        auto* row = mdb::Row::create(tbl_warehouse_->schema(), row_data);
        txn->insert_row(tbl_warehouse_, row);
        
        // Create districts for this warehouse
        for (uint32_t d = 0; d < num_districts_per_wh_; d++) {
            std::vector<mdb::Value> dist_data = {
                mdb::Value((i32)d),                       // d_id
                mdb::Value((i32)w),                       // d_w_id
                mdb::Value("District_" + std::to_string(d)), // d_name
                mdb::Value("DStreet1"),
                mdb::Value("DStreet2"),
                mdb::Value("DCity"),
                mdb::Value("ST"),
                mdb::Value("12345"),
                mdb::Value(0.0875),                       // d_tax
                mdb::Value(30000.0),                      // d_ytd
                mdb::Value((i32)3001)                     // d_next_o_id
            };
            auto* drow = mdb::Row::create(tbl_district_->schema(), dist_data);
            txn->insert_row(tbl_district_, drow);
        }
    }
    
    // Populate items (same on all shards)
    for (uint32_t i = 0; i < num_items_; i++) {
        std::vector<mdb::Value> item_data = {
            mdb::Value((i32)i),                        // i_id
            mdb::Value((i32)(i % 10000)),              // i_im_id
            mdb::Value("Item_" + std::to_string(i)),   // i_name
            mdb::Value(1.0 + (i % 100)),               // i_price
            mdb::Value("ItemData")                     // i_data
        };
        auto* row = mdb::Row::create(tbl_item_->schema(), item_data);
        txn->insert_row(tbl_item_, row);
    }
    
    delete txn;
    
    Log_info("LuigiTPCCStateMachine[%u]: Data populated", shard_id_);
}

bool LuigiTPCCStateMachine::Execute(uint32_t txn_type,
                                     const std::vector<LuigiOp>& ops,
                                     std::map<std::string, std::string>* output,
                                     uint64_t txn_id) {
    // TPC-C transaction types
    enum TPCCTxnType {
        NEW_ORDER = 0,
        PAYMENT = 1,
        ORDER_STATUS = 2,
        DELIVERY = 3,
        STOCK_LEVEL = 4
    };
    
    switch (txn_type) {
        case NEW_ORDER:
            return ExecuteNewOrder(ops, output, txn_id);
        case PAYMENT:
            return ExecutePayment(ops, output, txn_id);
        case ORDER_STATUS:
            return ExecuteOrderStatus(ops, output, txn_id);
        case DELIVERY:
            return ExecuteDelivery(ops, output, txn_id);
        case STOCK_LEVEL:
            return ExecuteStockLevel(ops, output, txn_id);
        default:
            Log_warn("Unknown TPC-C transaction type: %u", txn_type);
            return false;
    }
}

void LuigiTPCCStateMachine::MapOpsToShards(uint32_t txn_type,
                                            const std::vector<LuigiOp>& ops,
                                            std::map<uint32_t, std::vector<LuigiOp>>* shard_ops) {
    // TPC-C sharding is by warehouse
    // Extract w_id from op keys and route accordingly
    for (const auto& op : ops) {
        uint32_t target_shard = KeyToShard(op.key);
        (*shard_ops)[target_shard].push_back(op);
    }
}

uint32_t LuigiTPCCStateMachine::KeyToShard(const std::string& key) const {
    // TPC-C sharding: extract warehouse ID from key
    // Key format: "table_w<w_id>_..."
    
    // Simple approach: find "w" followed by digits
    size_t w_pos = key.find("_w");
    if (w_pos != std::string::npos) {
        w_pos += 2;  // Skip "_w"
        size_t end = w_pos;
        while (end < key.length() && isdigit(key[end])) {
            end++;
        }
        if (end > w_pos) {
            int w_id = std::stoi(key.substr(w_pos, end - w_pos));
            return w_id % shard_num_;
        }
    }
    
    // Fallback to parent implementation
    return LuigiStateMachine::KeyToShard(key);
}

//=============================================================================
// TPC-C Transaction Implementations (Simplified)
//=============================================================================

bool LuigiTPCCStateMachine::ExecuteNewOrder(const std::vector<LuigiOp>& ops,
                                             std::map<std::string, std::string>* output,
                                             uint64_t txn_id) {
    // New Order: Read warehouse, district; Update district; Read items, stock; 
    // Update stock; Insert order, new_order, order_line
    
    auto* txn = txn_mgr_.start(txn_id);
    
    // Process operations
    for (const auto& op : ops) {
        if (!IsLocalOp(op)) continue;
        
        if (op.op_type == LUIGI_OP_READ) {
            // Look up the row and return value
            // Key parsing would determine table and primary key
            // Simplified: just return empty for now
            if (output) {
                (*output)[op.key] = op.value;
            }
        } else if (op.op_type == LUIGI_OP_WRITE) {
            // Insert or update
            // Actual implementation would parse op and modify appropriate table
        }
    }
    
    delete txn;
    return true;
}

bool LuigiTPCCStateMachine::ExecutePayment(const std::vector<LuigiOp>& ops,
                                            std::map<std::string, std::string>* output,
                                            uint64_t txn_id) {
    // Payment: Update warehouse, district, customer; Insert history
    
    auto* txn = txn_mgr_.start(txn_id);
    
    for (const auto& op : ops) {
        if (!IsLocalOp(op)) continue;
        
        if (op.op_type == LUIGI_OP_READ) {
            if (output) {
                (*output)[op.key] = op.value;
            }
        } else if (op.op_type == LUIGI_OP_WRITE) {
            // Process update
        }
    }
    
    delete txn;
    return true;
}

bool LuigiTPCCStateMachine::ExecuteOrderStatus(const std::vector<LuigiOp>& ops,
                                                std::map<std::string, std::string>* output,
                                                uint64_t txn_id) {
    // Order Status: Read customer, order, order_line (read-only)
    
    auto* txn = txn_mgr_.start(txn_id);
    
    for (const auto& op : ops) {
        if (!IsLocalOp(op)) continue;
        
        if (op.op_type == LUIGI_OP_READ) {
            if (output) {
                (*output)[op.key] = op.value;
            }
        }
    }
    
    delete txn;
    return true;
}

bool LuigiTPCCStateMachine::ExecuteDelivery(const std::vector<LuigiOp>& ops,
                                             std::map<std::string, std::string>* output,
                                             uint64_t txn_id) {
    // Delivery: Read/delete new_order; Update order, customer
    
    auto* txn = txn_mgr_.start(txn_id);
    
    for (const auto& op : ops) {
        if (!IsLocalOp(op)) continue;
        
        if (op.op_type == LUIGI_OP_READ) {
            if (output) {
                (*output)[op.key] = op.value;
            }
        } else if (op.op_type == LUIGI_OP_WRITE) {
            // Process update/delete
        }
    }
    
    delete txn;
    return true;
}

bool LuigiTPCCStateMachine::ExecuteStockLevel(const std::vector<LuigiOp>& ops,
                                               std::map<std::string, std::string>* output,
                                               uint64_t txn_id) {
    // Stock Level: Read district, order_line, stock (read-only)
    
    auto* txn = txn_mgr_.start(txn_id);
    
    for (const auto& op : ops) {
        if (!IsLocalOp(op)) continue;
        
        if (op.op_type == LUIGI_OP_READ) {
            if (output) {
                (*output)[op.key] = op.value;
            }
        }
    }
    
    delete txn;
    return true;
}

mdb::Row* LuigiTPCCStateMachine::CreateRowFromOp(mdb::Table* tbl, const LuigiOp& op) {
    // Parse op.value into column values and create row
    // This would need proper serialization/deserialization
    // Placeholder for now
    return nullptr;
}

}  // namespace janus
