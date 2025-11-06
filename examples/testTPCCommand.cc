#include "../src/deptran/raft/raft_worker.h"
#include "../src/deptran/classic/tpc_command.h"
#include "../src/memdb/value.h"
#include <iostream>
#include <cassert>

using namespace janus;

// Simple test to verify CreateRaftLogCommand() produces correct structure
int main() {
    std::cout << "=== Testing CreateRaftLogCommand() ===" << std::endl;

    // Create a RaftWorker instance (minimal setup)
    RaftWorker worker;

    // Test data
    const char* test_log = "TEST_LOG_ENTRY_001";
    int test_len = strlen(test_log);
    txnid_t test_tx_id = 12345;

    std::cout << "Creating TpcCommitCommand with raw bytes: \"" << test_log << "\"" << std::endl;

    // Call our helper function
    auto tpc_cmd = worker.CreateRaftLogCommand(test_log, test_len, test_tx_id);

    // Verify the structure
    std::cout << "Step 1: Verify TpcCommitCommand is not null... ";
    assert(tpc_cmd != nullptr);
    std::cout << "✓" << std::endl;

    std::cout << "Step 2: Verify tx_id is set correctly... ";
    assert(tpc_cmd->tx_id_ == test_tx_id);
    std::cout << "✓ (tx_id=" << tpc_cmd->tx_id_ << ")" << std::endl;

    std::cout << "Step 3: Verify cmd_ is VecPieceData... ";
    auto vpd = std::dynamic_pointer_cast<VecPieceData>(tpc_cmd->cmd_);
    assert(vpd != nullptr);
    std::cout << "✓" << std::endl;

    std::cout << "Step 4: Verify VecPieceData has sp_vec_piece_data_... ";
    assert(vpd->sp_vec_piece_data_ != nullptr);
    assert(!vpd->sp_vec_piece_data_->empty());
    std::cout << "✓ (size=" << vpd->sp_vec_piece_data_->size() << ")" << std::endl;

    std::cout << "Step 5: Verify SimpleCommand exists... ";
    auto simple_cmd = (*vpd->sp_vec_piece_data_)[0];
    assert(simple_cmd != nullptr);
    std::cout << "✓" << std::endl;

    std::cout << "Step 6: Verify SimpleCommand has values_... ";
    assert(simple_cmd->input.values_ != nullptr);
    assert(!simple_cmd->input.values_->empty());
    std::cout << "✓ (num_values=" << simple_cmd->input.values_->size() << ")" << std::endl;

    std::cout << "Step 7: Verify Value is STR type... ";
    auto& first_val = simple_cmd->input.values_->begin()->second;
    assert(first_val.get_kind() == Value::STR);
    std::cout << "✓" << std::endl;

    std::cout << "Step 8: Verify raw bytes match original... ";
    const std::string& payload = first_val.get_str();
    assert(payload.size() == test_len);
    assert(memcmp(payload.c_str(), test_log, test_len) == 0);
    std::cout << "✓ (payload=\"" << payload << "\")" << std::endl;

    std::cout << "\n=== All tests passed! ===" << std::endl;
    std::cout << "CreateRaftLogCommand() correctly creates:" << std::endl;
    std::cout << "  TpcCommitCommand {" << std::endl;
    std::cout << "    tx_id_ = " << tpc_cmd->tx_id_ << std::endl;
    std::cout << "    cmd_ = VecPieceData {" << std::endl;
    std::cout << "      SimpleCommand {" << std::endl;
    std::cout << "        Value(STR) = \"" << payload << "\"" << std::endl;
    std::cout << "      }" << std::endl;
    std::cout << "    }" << std::endl;
    std::cout << "  }" << std::endl;

    return 0;
}
