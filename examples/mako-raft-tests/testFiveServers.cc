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

// Test: 5-Server Raft Cluster
//
// Goal: Verify that Raft works correctly with 5 nodes
//       - Majority = 3 out of 5 nodes
//       - Leader should wait for 2 follower ACKs (3 total including leader)
//       - All nodes should eventually reach consensus

// Simple tracking: Did we apply a log at slot 1?
atomic<bool> slot_1_applied{false};
atomic<int> total_applies{0};
string slot_1_content = "";

// Event-driven: Track when leader is elected
atomic<bool> leader_elected{false};

mutex cout_mutex;
mutex content_mutex;

void safe_print(const string& msg) {
    std::lock_guard<std::mutex> lock(cout_mutex);
    cout << msg << endl;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <process_name>" << endl;
        cerr << "  process_name: localhost, p1, p2, p3, or p4" << endl;
        return 1;
    }

    string proc_name = string(argv[1]);

    safe_print("=========================================");
    safe_print("Test: 5-Server Raft Cluster");
    safe_print("Process: " + proc_name);
    safe_print("=========================================");

    // Configuration
    vector<string> config{
        get_current_absolute_path() + "../config/none_raft.yml",
        get_current_absolute_path() + "../config/1c1s5r1p_cluster_test.yml"
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

    // Leader election callback
    register_leader_election_callback([&](int control) {
        leader_elected.store(true);
        safe_print("[" + proc_name + "] EVENT: Leader election detected (control=" + to_string(control) + ")!");
    });

    // Unified callback for both leader and follower
    auto unified_callback = [&](const char*& log, int len, int par_id, int slot_id,
                                queue<tuple<int, int, int, int, const char*>>& un_replay_logs_) {
        total_applies++;

        if (slot_id == 1 && !slot_1_applied.load()) {
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
        int status = 1;
        return static_cast<int>(timestamp * 10 + status);
    };

    register_for_leader_par_id_return(unified_callback, 0);
    register_for_follower_par_id_return(unified_callback, 0);

    safe_print("[" + proc_name + "] ✓ Callbacks registered");

    // Step 3: Launch services
    safe_print("[" + proc_name + "] Step 3: Calling setup2()...");
    setup2(0, 0);
    safe_print("[" + proc_name + "] ✓ setup2() completed");

    // Step 4: Wait for cluster to stabilize
    // CRITICAL: With 5 nodes, need more time for all to connect
    safe_print("[" + proc_name + "] Step 4: Waiting for cluster to stabilize (5s for 5 nodes)...");
    this_thread::sleep_for(chrono::seconds(5));

    // Now wait for leader election callback (opportunistic)
    int waited = 0;
    while (!leader_elected.load() && waited < 1000) {
        this_thread::sleep_for(chrono::milliseconds(100));
        waited += 100;
    }

    if (leader_elected.load()) {
        safe_print("[" + proc_name + "] ✓ Leader election detected after " + to_string(waited) + "ms");
    } else {
        safe_print("[" + proc_name + "] Waited " + to_string(waited) + "ms (this node is not leader)");
    }

    // Step 5: ALL nodes try to submit (only leader will succeed)
    safe_print("[" + proc_name + "] Step 5: Submitting log (only leader succeeds)...");

    const char* log_msg = "TEST_5_NODE_CLUSTER";
    int log_len = strlen(log_msg);

    add_log_to_nc(log_msg, log_len, 0);
    safe_print("[" + proc_name + "] Log submission attempted");

    // Step 6: Wait for log application
    safe_print("[" + proc_name + "] Step 6: Waiting for log application (max 5s)...");

    waited = 0;
    while (!slot_1_applied.load() && waited < 5000) {
        this_thread::sleep_for(chrono::milliseconds(100));
        waited += 100;
    }

    if (slot_1_applied.load()) {
        safe_print("[" + proc_name + "] ✓ Log applied after " + to_string(waited) + "ms");
    } else {
        safe_print("[" + proc_name + "] ✗ FAILED: Log not applied!");
        pre_shutdown_step();
        shutdown_paxos();
        return 1;
    }

    // Step 7: Check results
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
