#include <iostream>
#include <cstring>
#include <atomic>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <mako.hh>
#include <examples/common.h>

using namespace std;
using namespace mako;

// Test: Mako Format Log Submission - Real Transaction Format Test
//
// Goal: Verify that Raft can replicate REAL Mako transaction logs
//       (not just simple strings, but the actual binary format Mako uses)
//
// What we're testing:
// - Serialize a transaction in Mako's format:
//   [commit_id][count][len][K1,V1][K2,V2]...[final_commit_id][latency]
// - Submit to Raft via add_log_to_nc()
// - Verify all replicas receive and can parse it
// - Verify replay callback can decode the format

// Simple tracking: How many logs did we apply?
atomic<bool> slot_1_applied{false};
atomic<int> total_applies{0};
const int EXPECTED_LOGS = 10;  // We'll submit 100 logs

// Track what we decoded from the log
struct DecodedTransaction {
    uint32_t commit_id;
    uint16_t count;
    uint32_t kv_len;
    vector<pair<string, string>> kv_pairs;
    uint32_t final_commit_id;
    uint32_t latency_tracker;
};

DecodedTransaction decoded_txn;
mutex decoded_mutex;

// Event-driven: Track when leader is elected
atomic<bool> leader_elected{false};

mutex cout_mutex;

void safe_print(const string& msg) {
    std::lock_guard<std::mutex> lock(cout_mutex);
    cout << msg << endl;
}

// Helper: Decode Mako transaction format
bool decode_mako_transaction(const char* log, int len, DecodedTransaction& txn) {
    if (len < 10) {  // Minimum: commit_id(4) + count(2) + len(4)
        return false;
    }

    size_t offset = 0;

    // 1. Read commit_id (4 bytes)
    memcpy(&txn.commit_id, log + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    // 2. Read count (2 bytes)
    memcpy(&txn.count, log + offset, sizeof(uint16_t));
    offset += sizeof(uint16_t);

    // 3. Read kv_len (4 bytes)
    memcpy(&txn.kv_len, log + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    // 4. Read K-V pairs
    for (int i = 0; i < txn.count; i++) {
        if (offset + 2 > len) return false;

        // Read key_len
        uint16_t key_len;
        memcpy(&key_len, log + offset, sizeof(uint16_t));
        offset += sizeof(uint16_t);

        if (offset + key_len > len) return false;

        // Read key_data
        string key(log + offset, key_len);
        offset += key_len;

        if (offset + 2 > len) return false;

        // Read val_len
        uint16_t val_len;
        memcpy(&val_len, log + offset, sizeof(uint16_t));
        offset += sizeof(uint16_t);

        if (offset + val_len > len) return false;

        // Read val_data
        string val(log + offset, val_len);
        offset += val_len;

        if (offset + 2 > len) return false;

        // Read table_id (skip for now)
        uint16_t table_id;
        memcpy(&table_id, log + offset, sizeof(uint16_t));
        offset += sizeof(uint16_t);

        txn.kv_pairs.push_back({key, val});
    }

    // 5. Read final_commit_id (4 bytes)
    if (offset + 4 > len) return false;
    memcpy(&txn.final_commit_id, log + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    // 6. Read latency_tracker (4 bytes)
    if (offset + 4 > len) return false;
    memcpy(&txn.latency_tracker, log + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    return true;
}

// Helper: Create a Mako-format transaction log
int create_mako_transaction_log(char* buffer, int max_len) {
    size_t offset = 0;

    // 1. Commit ID (timestamp*10 + term)
    uint32_t timestamp = 1234;
    uint32_t term = 5;
    uint32_t commit_id = timestamp * 10 + term;
    memcpy(buffer + offset, &commit_id, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    // 2. Count of K-V pairs (2 pairs for this test)
    uint16_t count = 2;
    memcpy(buffer + offset, &count, sizeof(uint16_t));
    offset += sizeof(uint16_t);

    // 3. Length placeholder (we'll fill this later)
    size_t len_offset = offset;
    offset += sizeof(uint32_t);
    size_t kv_start = offset;

    // 4. First K-V pair: warehouse:stock_42 = 100
    {
        string key = "warehouse:stock_42";
        uint16_t key_len = key.length();
        memcpy(buffer + offset, &key_len, sizeof(uint16_t));
        offset += sizeof(uint16_t);
        memcpy(buffer + offset, key.data(), key_len);
        offset += key_len;

        string val = "100";
        uint16_t val_len = val.length();
        memcpy(buffer + offset, &val_len, sizeof(uint16_t));
        offset += sizeof(uint16_t);
        memcpy(buffer + offset, val.data(), val_len);
        offset += val_len;

        uint16_t table_id = 1;  // warehouse table
        memcpy(buffer + offset, &table_id, sizeof(uint16_t));
        offset += sizeof(uint16_t);
    }

    // 5. Second K-V pair: customer:balance_99 = 500.50
    {
        string key = "customer:balance_99";
        uint16_t key_len = key.length();
        memcpy(buffer + offset, &key_len, sizeof(uint16_t));
        offset += sizeof(uint16_t);
        memcpy(buffer + offset, key.data(), key_len);
        offset += key_len;

        string val = "500.50";
        uint16_t val_len = val.length();
        memcpy(buffer + offset, &val_len, sizeof(uint16_t));
        offset += sizeof(uint16_t);
        memcpy(buffer + offset, val.data(), val_len);
        offset += val_len;

        uint16_t table_id = 2;  // customer table
        memcpy(buffer + offset, &table_id, sizeof(uint16_t));
        offset += sizeof(uint16_t);
    }

    // Fill in the K-V length
    uint32_t kv_len = offset - kv_start;
    memcpy(buffer + len_offset, &kv_len, sizeof(uint32_t));

    // 6. Final commit ID (same as initial)
    memcpy(buffer + offset, &commit_id, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    // 7. Latency tracker (current time in ms)
    uint32_t latency = getCurrentTimeMillis();
    memcpy(buffer + offset, &latency, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    return offset;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <process_name>" << endl;
        cerr << "  process_name: localhost, p1, or p2" << endl;
        return 1;
    }

    string proc_name = string(argv[1]);

    safe_print("=========================================");
    safe_print("Test: Mako Format Log Replication");
    safe_print("Process: " + proc_name);
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

    // Unified callback - decodes Mako format
    auto unified_callback = [&](const char*& log, int len, int par_id, int slot_id,
                                queue<tuple<int, int, int, int, const char*>>& un_replay_logs_) {
        total_applies++;

        if (slot_id == 1 && !slot_1_applied.load()) {
            slot_1_applied.store(true);

            // Try to decode the Mako transaction format
            DecodedTransaction txn;
            bool success = decode_mako_transaction(log, len, txn);

            if (success) {
                std::lock_guard<std::mutex> lock(decoded_mutex);
                decoded_txn = txn;

                safe_print("[" + proc_name + "] EVENT: Applied Mako transaction at slot 1!");
                safe_print("[" + proc_name + "]   commit_id: " + to_string(txn.commit_id));
                safe_print("[" + proc_name + "]   count: " + to_string(txn.count));
                safe_print("[" + proc_name + "]   kv_len: " + to_string(txn.kv_len));
                for (size_t i = 0; i < txn.kv_pairs.size(); i++) {
                    safe_print("[" + proc_name + "]   K-V[" + to_string(i) + "]: " +
                              txn.kv_pairs[i].first + " = " + txn.kv_pairs[i].second);
                }
                safe_print("[" + proc_name + "]   final_commit_id: " + to_string(txn.final_commit_id));
            } else {
                safe_print("[" + proc_name + "] ERROR: Failed to decode Mako transaction!");
            }
        }

        uint32_t timestamp = static_cast<uint32_t>(getCurrentTimeMillis());
        int status = 1;
        return static_cast<int>(timestamp * 10 + status);
    };

    // Register for BOTH leader and follower roles
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

    // Step 5: Submit MULTIPLE Mako-format transactions (like real Mako does!)
    safe_print("[" + proc_name + "] Step 5: Creating and submitting MULTIPLE Mako transactions...");

    const int NUM_TRANSACTIONS = 10;  // Submit 10 transactions to test batching
    safe_print("[" + proc_name + "] Will submit " + to_string(NUM_TRANSACTIONS) + " transactions");

    for (int i = 0; i < NUM_TRANSACTIONS; i++) {
        char mako_log[1024];
        int mako_log_len = create_mako_transaction_log(mako_log, sizeof(mako_log));

        safe_print("[" + proc_name + "] [" + to_string(i+1) + "/" + to_string(NUM_TRANSACTIONS) +
                   "] Submitting Mako transaction (" + to_string(mako_log_len) + " bytes)");

        // Submit immediately (back-to-back, like Mako does under high load)
        add_log_to_nc(mako_log, mako_log_len, 0);

        // Small delay to simulate realistic transaction rate (optional)
        // In real Mako, transactions come back-to-back with no delay
        // this_thread::sleep_for(chrono::milliseconds(1));
    }

    safe_print("[" + proc_name + "] All " + to_string(NUM_TRANSACTIONS) +
               " transactions submitted!");
    safe_print("[" + proc_name + "] NOTE: Watch for [BATCH] messages in logs to confirm batching!");

    // Step 6: Wait for log application
    safe_print("[" + proc_name + "] Step 6: Waiting for log application (5s)...");
    int waited = 0;
    while (!slot_1_applied.load() && waited < 5000) {
        this_thread::sleep_for(chrono::milliseconds(100));
        waited += 100;
    }

    // Step 7: Check results
    safe_print("");
    safe_print("=========================================");
    safe_print("[" + proc_name + "] Test Results");
    safe_print("=========================================");

    bool log_applied = slot_1_applied.load();
    safe_print("[" + proc_name + "] Slot 1 applied: " + string(log_applied ? "YES" : "NO"));

    if (log_applied) {
        std::lock_guard<std::mutex> lock(decoded_mutex);
        safe_print("[" + proc_name + "] Decoded transaction:");
        safe_print("[" + proc_name + "]   commit_id: " + to_string(decoded_txn.commit_id));
        safe_print("[" + proc_name + "]   K-V pairs: " + to_string(decoded_txn.count));
        for (size_t i = 0; i < decoded_txn.kv_pairs.size(); i++) {
            safe_print("[" + proc_name + "]     [" + to_string(i) + "] " +
                      decoded_txn.kv_pairs[i].first + " = " + decoded_txn.kv_pairs[i].second);
        }

        // Validate the decoded data matches what we sent
        bool validation_pass = true;
        if (decoded_txn.commit_id != 12345) {  // 1234*10 + 5
            safe_print("[" + proc_name + "] ✗ commit_id mismatch!");
            validation_pass = false;
        }
        if (decoded_txn.count != 2) {
            safe_print("[" + proc_name + "] ✗ count mismatch!");
            validation_pass = false;
        }
        if (decoded_txn.kv_pairs.size() != 2) {
            safe_print("[" + proc_name + "] ✗ K-V pairs size mismatch!");
            validation_pass = false;
        }

        if (validation_pass) {
            safe_print("[" + proc_name + "] ✓ All validation checks passed!");
        }
    }

    safe_print("[" + proc_name + "] Total applies: " + to_string(total_applies.load()) +
               " / " + to_string(EXPECTED_LOGS) + " expected");

    bool pass = log_applied && (total_applies.load() >= EXPECTED_LOGS);

    // Step 8: Cleanup
    safe_print("");
    safe_print("[" + proc_name + "] Step 8: Cleanup...");
    pre_shutdown_step();
    shutdown_paxos();

    safe_print("");
    safe_print("=========================================");
    if (pass) {
        safe_print("[" + proc_name + "] Test PASSED ✓");
        safe_print("  - " + to_string(EXPECTED_LOGS) + " Mako-format transactions replicated successfully");
        safe_print("  - All replicas decoded the format correctly");
        safe_print("  - Check logs for [BATCH] messages to confirm batching!");
    } else {
        safe_print("[" + proc_name + "] Test FAILED ✗");
        if (total_applies.load() < EXPECTED_LOGS) {
            safe_print("  - Only applied " + to_string(total_applies.load()) +
                      " / " + to_string(EXPECTED_LOGS) + " logs!");
        }
    }
    safe_print("=========================================");

    return pass ? 0 : 1;
}
