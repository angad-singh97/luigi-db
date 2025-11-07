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

// Test: Single Log Submission - End-to-End Replication Test (FIXED LEADER MODE)
//
// Goal: Verify that a log submitted by the leader is replicated to ALL nodes
//       and that all nodes reach consensus (same log at same slot)
//
// Leadership Mode: FIXED LEADER (Paxos-style)
//   - localhost (machine_id=0) is forced as permanent leader
//   - p1 and p2 are permanent followers
//   - No elections occur during the test
//   - This is enforced by setup2() in raft_main_helper.cc
//
// We test:
// - Log is applied at slot 1 on THIS node (regardless of role)
// - Consensus: All nodes have the same log content
// - No crashes or segfaults
// - Leader (localhost) successfully submits and replicates logs
// - Followers (p1, p2) receive and apply replicated logs

// Simple tracking: Did we apply a log at slot 1?
atomic<bool> slot_1_applied{false};
atomic<int> total_applies{0};
string slot_1_content = "";  // Store what we applied at slot 1

// Event-driven: Track when leader is elected
atomic<bool> leader_elected{false};

mutex cout_mutex;
mutex content_mutex;  // Protect slot_1_content

void safe_print(const string& msg) {
    std::lock_guard<std::mutex> lock(cout_mutex);
    cout << msg << endl;
}

// Helper: Wait for leader election (event-driven)
bool wait_for_leader_election(const string& proc_name, int max_wait_ms = 10000) {
    safe_print("[" + proc_name + "] Waiting for leader election (max " + to_string(max_wait_ms/1000) + "s)...");

    int waited = 0;
    while (!leader_elected.load() && waited < max_wait_ms) {
        this_thread::sleep_for(chrono::milliseconds(100));
        waited += 100;
    }

    if (leader_elected.load()) {
        safe_print("[" + proc_name + "] ✓ Leader elected after " + to_string(waited) + "ms");
        return true;
    } else {
        safe_print("[" + proc_name + "] ✗ No leader elected within " + to_string(max_wait_ms) + "ms");
        return false;
    }
}

// Helper: Wait for log application (polling)
bool wait_for_log_application(const string& proc_name, int slot, int max_wait_ms = 5000) {
    safe_print("[" + proc_name + "] Waiting for log at slot " + to_string(slot) + " (max " + to_string(max_wait_ms/1000) + "s)...");

    int waited = 0;
    while (!slot_1_applied.load() && waited < max_wait_ms) {
        this_thread::sleep_for(chrono::milliseconds(100));
        waited += 100;
    }

    if (slot_1_applied.load()) {
        safe_print("[" + proc_name + "] ✓ Log applied after " + to_string(waited) + "ms");
        return true;
    } else {
        safe_print("[" + proc_name + "] ✗ Log not applied within " + to_string(max_wait_ms) + "ms");
        return false;
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <process_name>" << endl;
        cerr << "  process_name: localhost, p1, or p2" << endl;
        return 1;
    }

    string proc_name = string(argv[1]);

    safe_print("=========================================");
    safe_print("Test: Single Log Replication (End-to-End)");
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

    // Step 2: Register callbacks and leader election callback
    safe_print("[" + proc_name + "] Step 2: Registering callbacks...");

    // Leader election callback - fires when ANY node becomes leader
    // Note: callback fires with control == 0 when leader is elected
    register_leader_election_callback([&](int control) {
        leader_elected.store(true);
        safe_print("[" + proc_name + "] EVENT: Leader election detected (control=" + to_string(control) + ")!");
    });

    // Unified callback - doesn't care about role, just tracks log application
    auto unified_callback = [&](const char*& log, int len, int par_id, int slot_id,
                                queue<tuple<int, int, int, int, const char*>>& un_replay_logs_) {
        total_applies++;

        if (slot_id == 1 && !slot_1_applied.load()) {
            // First time seeing slot 1 - record it!
            slot_1_applied.store(true);

            {
                std::lock_guard<std::mutex> lock(content_mutex);
                if (len > 0 && len <= 100) {
                    slot_1_content = string(log, len);
                } else if (len > 0) {
                    slot_1_content = string(log, min(len, 50)) + "...(truncated)";
                }
            }

            safe_print("[" + proc_name + "] EVENT: Applied log at slot 1: '" + slot_1_content + "'");
        }

        // Return success status
        uint32_t timestamp = static_cast<uint32_t>(getCurrentTimeMillis());
        int status = 1;  // STATUS_NORMAL or STATUS_REPLAY_DONE
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

    // Step 4: Wait for cluster to stabilize and elect a leader
    // CRITICAL: Give ALL nodes time to complete setup2() and start their services
    // before any node tries to submit logs. This prevents connection blocking issues.
    safe_print("[" + proc_name + "] Step 4: Waiting for cluster to stabilize (3s)...");
    this_thread::sleep_for(chrono::seconds(3));

    // Now wait for leader election callback (opportunistic, not required)
    int waited = 0;
    while (!leader_elected.load() && waited < 1000) {
        this_thread::sleep_for(chrono::milliseconds(100));
        waited += 100;
    }

    if (leader_elected.load()) {
        safe_print("[" + proc_name + "] ✓ Leader election detected after " + to_string(waited) + "ms");
    } else {
        safe_print("[" + proc_name + "] Waited " + to_string(waited) + "ms for stabilization (this node is not leader)");
    }

    // Step 5: ALL nodes try to submit (only leader will succeed, others silently drop)
    safe_print("[" + proc_name + "] Step 5: Submitting log (only leader succeeds)...");

    const char* log_msg = "TEST_LOG_ENTRY_001";
    int log_len = strlen(log_msg);

    // Try to submit - will only work if we're currently leader
    add_log_to_nc(log_msg, log_len, 0);
    safe_print("[" + proc_name + "] Log submission attempted");

    // Step 6: Wait for log application (POLLING)
    safe_print("[" + proc_name + "] Step 6: Waiting for log application...");
    if (!wait_for_log_application(proc_name, 1, 5000)) {
        safe_print("[" + proc_name + "] ✗ FAILED: Log not applied!");
        pre_shutdown_step();
        shutdown_paxos();
        return 1;
    }

    // Step 7: Check results - SIMPLE!
    safe_print("");
    safe_print("=========================================");
    safe_print("[" + proc_name + "] Test Results");
    safe_print("=========================================");

    bool log_applied = slot_1_applied.load();
    string final_content;
    {
        std::lock_guard<std::mutex> lock(content_mutex);
        final_content = slot_1_content;
    }

    safe_print("[" + proc_name + "] Slot 1 applied: " + string(log_applied ? "YES" : "NO"));
    if (log_applied) {
        safe_print("[" + proc_name + "] Slot 1 content: '" + final_content + "'");
        safe_print("[" + proc_name + "] Total applies: " + to_string(total_applies.load()));
    }

    // Print consensus marker for shell script to parse
    safe_print("[" + proc_name + "] CONSENSUS_CHECK: " +
               (log_applied ? "slot=1,content=" + final_content : "NONE"));

    bool pass = log_applied;

    // Step 8: Cleanup
    safe_print("");
    safe_print("[" + proc_name + "] Step 8: Cleanup...");
    pre_shutdown_step();
    shutdown_paxos();

    safe_print("");
    safe_print("=========================================");
    if (pass) {
        safe_print("[" + proc_name + "] Test PASSED ✓");
    } else {
        safe_print("[" + proc_name + "] Test FAILED ✗");
    }
    safe_print("=========================================");

    return pass ? 0 : 1;
}
