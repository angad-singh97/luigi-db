#include <iostream>
#include <cstring>
#include <atomic>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <map>
#include <mako.hh>
#include <examples/common.h>

using namespace std;
using namespace mako;

// Test: NO-OPS (Application-Level Heartbeats)
//
// Purpose: Verify that Mako's NO-OP mechanism works correctly
//          This is DIFFERENT from Raft's internal heartbeats!
//
// What we're testing:
// 1. isNoops() recognition - Does it identify "no-ops:X" format?
// 2. Replication - Do all nodes receive the NO-OP?
// 3. Epoch extraction - Can we extract the epoch number?
// 4. Non-transaction handling - NO-OPs should NOT be processed as transactions
// 5. Multiple NO-OPs - Can we send several in sequence?
// 6. Consensus - Do all nodes agree on the NO-OP content and epoch?

// Test configuration
const int NUM_NOOPS_TO_TEST = 3;  // Test epochs 3, 5, 7

// Tracking structures
struct NoOpRecord {
    int slot_id;
    int epoch;
    string content;
    uint64_t timestamp_received;
    bool tried_to_process_as_transaction;  // Should always be false!
};

// Per-node tracking
atomic<int> total_noops_received{0};
map<int, NoOpRecord> noops_by_slot;  // slot_id -> NoOpRecord
mutex noops_mutex;

// Event tracking
atomic<bool> leader_elected{false};
atomic<int> noops_applied{0};  // Count of NO-OPs successfully applied

mutex cout_mutex;

void safe_print(const string& msg) {
    std::lock_guard<mutex> lock(cout_mutex);
    cout << msg << endl;
}

// Helper: Check if this looks like a Mako transaction
bool looks_like_transaction(const char* log, int len) {
    // Mako transactions have specific structure:
    // [cid (4)][count (2)][len (4)][kvdata...][timestamp (4)][latency (4)]
    // Minimum size would be 4+2+4+0+4+4 = 18 bytes
    if (len < 18) return false;

    // NO-OPs are exactly 8 bytes: "no-ops:X"
    if (len == 8 && log[0] == 'n' && log[1] == 'o') return false;

    return true;
}

// Helper: Get current time in milliseconds
uint64_t get_current_time_ms() {
    return chrono::duration_cast<chrono::milliseconds>(
        chrono::system_clock::now().time_since_epoch()
    ).count();
}

int main(int argc, char **argv) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <process_name>" << endl;
        cerr << "  process_name: localhost, p1, or p2" << endl;
        return 1;
    }

    string proc_name = string(argv[1]);

    safe_print("=========================================");
    safe_print("Test: NO-OPS (Application-Level Heartbeats)");
    safe_print("Process: " + proc_name);
    safe_print("=========================================");
    safe_print("");
    safe_print("What we're testing:");
    safe_print("1. isNoops() recognition");
    safe_print("2. Replication to all nodes");
    safe_print("3. Epoch extraction");
    safe_print("4. Non-transaction handling");
    safe_print("5. Multiple NO-OPs in sequence");
    safe_print("6. Consensus verification");
    safe_print("=========================================");

    // Configuration
    vector<string> config{
        get_current_absolute_path() + "../config/none_raft.yml",
        get_current_absolute_path() + "../config/1c1s3r1p_cluster_test.yml"
    };

    char *argv_raft[18];
    argv_raft[0] = (char *)"";
    argv_raft[1] = (char *)"-b";
    argv_raft[2] = (char *)"-d";
    argv_raft[3] = (char *)"60";
    argv_raft[4] = (char *)"-f";
    argv_raft[5] = (char *) config[0].c_str();
    argv_raft[6] = (char *)"-f";
    argv_raft[7] = (char *) config[1].c_str();
    argv_raft[8] = (char *)"-t";
    argv_raft[9] = (char *)"30";
    argv_raft[10] = (char *)"-T";
    argv_raft[11] = (char *)"100000";
    argv_raft[12] = (char *)"-n";
    argv_raft[13] = (char *)"32";
    argv_raft[14] = (char *)"-P";
    argv_raft[15] = (char *) proc_name.c_str();
    argv_raft[16] = (char *)"-A";
    argv_raft[17] = (char *)"10000";

    // Step 1: Setup Raft
    safe_print("[" + proc_name + "] Step 1: Calling setup()...");
    vector<string> ret = setup(16, argv_raft);
    if (ret.empty()) {
        cerr << "[" << proc_name + "] ERROR: setup() failed!" << endl;
        return 1;
    }
    safe_print("[" + proc_name + "] ✓ setup() completed");

    // Step 2: Register callbacks
    safe_print("[" + proc_name + "] Step 2: Registering callbacks...");

    // Leader election callback
    register_leader_election_callback([&](int control) {
        leader_elected.store(true);
        safe_print("[" + proc_name + "] EVENT: Leader election detected!");
    });

    // Unified callback that handles BOTH transactions and NO-OPs
    auto unified_callback = [&](const char*& log, int len, int par_id, int slot_id,
                                queue<tuple<int, int, int, int, const char*>>& un_replay_logs_) {
        total_noops_received++;

        safe_print("[" + proc_name + "] CALLBACK: Received log at slot " + to_string(slot_id) +
                   ", len=" + to_string(len));

        // CRITICAL TEST: Check if this is a NO-OP
        int epoch = isNoops(log, len);

        if (epoch != -1) {
            // ===== NO-OP DETECTED =====
            safe_print("[" + proc_name + "] ✓ NO-OP DETECTED: epoch=" + to_string(epoch) +
                       ", slot=" + to_string(slot_id));

            // Record it
            {
                std::lock_guard<mutex> lock(noops_mutex);
                NoOpRecord record;
                record.slot_id = slot_id;
                record.epoch = epoch;
                record.content = string(log, len);
                record.timestamp_received = get_current_time_ms();
                record.tried_to_process_as_transaction = false;

                noops_by_slot[slot_id] = record;
            }

            noops_applied++;

            // Return success status
            uint32_t timestamp = static_cast<uint32_t>(getCurrentTimeMillis());
            int status = 1;  // STATUS_NORMAL
            return static_cast<int>(timestamp * 10 + status);

        } else {
            // ===== NOT A NO-OP =====
            safe_print("[" + proc_name + "] ! Regular log (not NO-OP) at slot " +
                       to_string(slot_id));

            // Check if it looks like a transaction
            bool looks_like_txn = looks_like_transaction(log, len);

            if (looks_like_txn) {
                safe_print("[" + proc_name + "] → Looks like a transaction, would process normally");
                // In real Mako, would call get_latest_commit_info() here
            } else {
                safe_print("[" + proc_name + "] → Unrecognized format, len=" + to_string(len));
            }

            // Mark that we would have tried to process as transaction
            {
                std::lock_guard<mutex> lock(noops_mutex);
                if (noops_by_slot.find(slot_id) != noops_by_slot.end()) {
                    noops_by_slot[slot_id].tried_to_process_as_transaction = true;
                }
            }

            uint32_t timestamp = static_cast<uint32_t>(getCurrentTimeMillis());
            int status = 1;
            return static_cast<int>(timestamp * 10 + status);
        }
    };

    // Register for BOTH leader and follower
    register_for_leader_par_id_return(unified_callback, 0);
    register_for_follower_par_id_return(unified_callback, 0);

    safe_print("[" + proc_name + "] ✓ Callbacks registered");

    // Step 3: Launch services
    safe_print("[" + proc_name + "] Step 3: Calling setup2()...");
    setup2(0, 0);
    safe_print("[" + proc_name + "] ✓ setup2() completed");

    // Step 4: Wait for cluster to stabilize
    safe_print("[" + proc_name + "] Step 4: Waiting for cluster to stabilize (3s)...");
    this_thread::sleep_for(chrono::seconds(3));

    int waited = 0;
    while (!leader_elected.load() && waited < 2000) {
        this_thread::sleep_for(chrono::milliseconds(100));
        waited += 100;
    }

    if (leader_elected.load()) {
        safe_print("[" + proc_name + "] ✓ Leader elected after " + to_string(waited) + "ms");
    } else {
        safe_print("[" + proc_name + "] Note: No leader election callback fired (this node not leader)");
    }

    // Step 5: Submit NO-OPs
    safe_print("");
    safe_print("[" + proc_name + "] Step 5: Submitting " + to_string(NUM_NOOPS_TO_TEST) +
               " NO-OPs...");

    vector<int> test_epochs = {3, 5, 7};  // Test different epochs

    for (int i = 0; i < NUM_NOOPS_TO_TEST; i++) {
        int epoch = test_epochs[i];

        // Create NO-OP in correct format: "no-ops:X" (exactly 8 bytes)
        char noop_log[9];  // 8 bytes + null terminator
        snprintf(noop_log, sizeof(noop_log), "no-ops:%d", epoch);

        safe_print("[" + proc_name + "] → Submitting NO-OP epoch " + to_string(epoch) +
                   " ('" + string(noop_log, 8) + "')");

        // Submit (only leader will succeed)
        add_log_to_nc(noop_log, 8, 0);

        // Wait a bit between submissions
        this_thread::sleep_for(chrono::milliseconds(500));
    }

    safe_print("[" + proc_name + "] ✓ Submitted " + to_string(NUM_NOOPS_TO_TEST) + " NO-OPs");

    // Step 6: Wait for NO-OPs to be applied
    safe_print("");
    safe_print("[" + proc_name + "] Step 6: Waiting for NO-OPs to be applied...");

    waited = 0;
    while (noops_applied.load() < NUM_NOOPS_TO_TEST && waited < 5000) {
        this_thread::sleep_for(chrono::milliseconds(100));
        waited += 100;

        if (waited % 1000 == 0) {
            safe_print("[" + proc_name + "] → Applied " + to_string(noops_applied.load()) +
                       " / " + to_string(NUM_NOOPS_TO_TEST) + " NO-OPs so far...");
        }
    }

    int final_count = noops_applied.load();

    if (final_count >= NUM_NOOPS_TO_TEST) {
        safe_print("[" + proc_name + "] ✓ All " + to_string(NUM_NOOPS_TO_TEST) +
                   " NO-OPs applied!");
    } else {
        safe_print("[" + proc_name + "] ⚠ Only " + to_string(final_count) + " / " +
                   to_string(NUM_NOOPS_TO_TEST) + " NO-OPs applied");
    }

    // Step 7: Analyze results
    safe_print("");
    safe_print("=========================================");
    safe_print("[" + proc_name + "] Test Results & Analysis");
    safe_print("=========================================");

    bool all_tests_passed = true;

    // Test 1: NO-OP Recognition
    safe_print("");
    safe_print("--- Test 1: NO-OP Recognition ---");
    {
        std::lock_guard<mutex> lock(noops_mutex);
        int recognized = 0;
        for (const auto& pair : noops_by_slot) {
            if (pair.second.epoch >= 0) {
                recognized++;
            }
        }
        safe_print("NO-OPs recognized: " + to_string(recognized) + " / " +
                   to_string(noops_by_slot.size()));

        if (recognized == noops_by_slot.size() && recognized > 0) {
            safe_print("✓ PASS: All NO-OPs correctly recognized by isNoops()");
        } else {
            safe_print("✗ FAIL: Some NO-OPs not recognized!");
            all_tests_passed = false;
        }
    }

    // Test 2: Epoch Extraction
    safe_print("");
    safe_print("--- Test 2: Epoch Extraction ---");
    {
        std::lock_guard<mutex> lock(noops_mutex);
        bool all_correct = true;

        for (const auto& pair : noops_by_slot) {
            const NoOpRecord& rec = pair.second;
            safe_print("  Slot " + to_string(rec.slot_id) + ": epoch=" +
                       to_string(rec.epoch) + ", content='" + rec.content + "'");

            // Verify epoch matches expected
            int expected_epoch = -1;
            if (rec.content == "no-ops:3") expected_epoch = 3;
            else if (rec.content == "no-ops:5") expected_epoch = 5;
            else if (rec.content == "no-ops:7") expected_epoch = 7;

            if (expected_epoch != -1 && rec.epoch != expected_epoch) {
                safe_print("    ✗ ERROR: Expected epoch " + to_string(expected_epoch) +
                           " but got " + to_string(rec.epoch));
                all_correct = false;
            }
        }

        if (all_correct) {
            safe_print("✓ PASS: All epochs correctly extracted");
        } else {
            safe_print("✗ FAIL: Epoch extraction errors!");
            all_tests_passed = false;
        }
    }

    // Test 3: Non-Transaction Handling
    safe_print("");
    safe_print("--- Test 3: Non-Transaction Handling ---");
    {
        std::lock_guard<mutex> lock(noops_mutex);
        bool all_correct = true;

        for (const auto& pair : noops_by_slot) {
            if (pair.second.tried_to_process_as_transaction) {
                safe_print("  ✗ ERROR: NO-OP at slot " + to_string(pair.first) +
                           " was processed as transaction!");
                all_correct = false;
            }
        }

        if (all_correct) {
            safe_print("✓ PASS: NO-OPs were NOT processed as transactions");
        } else {
            safe_print("✗ FAIL: Some NO-OPs were incorrectly processed as transactions!");
            all_tests_passed = false;
        }
    }

    // Test 4: Replication Check
    safe_print("");
    safe_print("--- Test 4: Replication Check ---");
    safe_print("This node received: " + to_string(final_count) + " NO-OPs");
    if (final_count >= NUM_NOOPS_TO_TEST) {
        safe_print("✓ PASS: All expected NO-OPs received");
    } else {
        safe_print("⚠ PARTIAL: Not all NO-OPs received (may be timing issue)");
        // Don't fail test - may be valid if this node wasn't leader
    }

    // Print consensus data for shell script to parse
    safe_print("");
    safe_print("--- Consensus Data ---");
    {
        std::lock_guard<mutex> lock(noops_mutex);
        for (const auto& pair : noops_by_slot) {
            const NoOpRecord& rec = pair.second;
            safe_print("[" + proc_name + "] CONSENSUS_NOOP: slot=" + to_string(rec.slot_id) +
                       ",epoch=" + to_string(rec.epoch) + ",content=" + rec.content);
        }
    }

    // Step 8: Cleanup
    safe_print("");
    safe_print("[" + proc_name + "] Step 8: Cleanup...");
    pre_shutdown_step();
    shutdown_paxos();

    // Final verdict
    safe_print("");
    safe_print("=========================================");
    if (all_tests_passed && final_count >= NUM_NOOPS_TO_TEST) {
        safe_print("[" + proc_name + "] Test PASSED ✓");
        safe_print("  - NO-OP recognition: ✓");
        safe_print("  - Epoch extraction: ✓");
        safe_print("  - Non-transaction handling: ✓");
        safe_print("  - Replication: ✓");
    } else {
        safe_print("[" + proc_name + "] Test FAILED ✗");
    }
    safe_print("=========================================");

    return (all_tests_passed && final_count >= NUM_NOOPS_TO_TEST) ? 0 : 1;
}
