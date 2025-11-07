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

// Test 1.5: Election Stability Test
// Purpose: Validate stable leader election without log submission
//
// What each PROCESS tests:
// - Whether THIS node became leader (via leader_election_callback)
// - No crashes during election
// - Reports final role and leader change count
//
// What the SHELL SCRIPT validates (cluster-wide):
// - Exactly one node reports FINAL_ROLE=LEADER (no split-brain)
// - Leadership is stable (not multiple nodes becoming leader)
//
// What we DON'T test:
// - Log submission (that's test02)
// - Callbacks execution
// - Replication
//
// Note: Individual processes always "pass" - they just report facts.
// The shell script aggregates all 3 logs to determine cluster-wide stability.

atomic<bool> became_leader{false};
atomic<bool> lost_leadership{false};
atomic<int> leader_changes{0};
atomic<int> current_term{0};

mutex cout_mutex;  // Protect cout from multiple threads

void safe_print(const string& msg) {
    std::lock_guard<std::mutex> lock(cout_mutex);
    cout << msg << endl;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <process_name>" << endl;
        cerr << "  process_name: localhost, p1, or p2" << endl;
        return 1;
    }

    string proc_name = string(argv[1]);

    safe_print("=========================================");
    safe_print("Test 1.5: Election Stability Test");
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
        cerr << "[" << proc_name << "] ERROR: setup() failed!" << endl;
        return 1;
    }
    safe_print("[" + proc_name + "] ✓ setup() completed");

    // Step 2: Register leader election callback
    safe_print("[" + proc_name + "] Step 2: Registering leader election callback...");

    register_leader_election_callback([&](int control) {
        leader_changes++;
        bool was_leader = became_leader.load();

        if (!was_leader) {
            // First time becoming leader
            became_leader.store(true);
            safe_print("[" + proc_name + "] *** BECAME LEADER *** (control=" + to_string(control) + ")");
        } else {
            // Already was leader, this is a re-election or confirmation
            safe_print("[" + proc_name + "] *** LEADERSHIP CONFIRMED *** (control=" + to_string(control) + ", changes=" + to_string(leader_changes.load()) + ")");
        }
    });

    safe_print("[" + proc_name + "] ✓ Leader election callback registered");

    // Step 3: Launch services
    safe_print("[" + proc_name + "] Step 3: Calling setup2()...");
    setup2(0, 0);
    safe_print("[" + proc_name + "] ✓ setup2() completed");

    // Step 4: Wait for initial leader election
    safe_print("[" + proc_name + "] Step 4: Waiting for leader election (3s)...");
    sleep(3);

    // Step 5: Report initial status
    safe_print("");
    safe_print("=========================================");
  safe_print("[" + proc_name + "] Initial Election Results (T+3s)");
  safe_print("=========================================");
  if (became_leader.load()) {
    safe_print("[" + proc_name + "] Role: LEADER");
    safe_print("[" + proc_name + "] Leader changes: " + to_string(leader_changes.load()));
  } else {
    safe_print("[" + proc_name + "] Role: FOLLOWER");
    safe_print("[" + proc_name + "] (No leader election callback fired)");
  }
  safe_print("");

  // Step 6: Monitor stability for 5 more seconds
    safe_print("[" + proc_name + "] Step 5: Monitoring stability (5s)...");

    int prev_changes = leader_changes.load();

    for (int i = 0; i < 5; i++) {
        sleep(1);
        int curr_changes = leader_changes.load();

        if (became_leader.load()) {
            if (curr_changes > prev_changes) {
                safe_print("[" + proc_name + "] T+" + to_string(4+i) + "s: Leadership changes detected (" + to_string(curr_changes) + " total)");
                prev_changes = curr_changes;
            }
        }
    }

    // Step 7: Final report
    safe_print("");
    safe_print("=========================================");
    safe_print("[" + proc_name + "] Final Test Results");
    safe_print("=========================================");

    int final_changes = leader_changes.load();

    if (became_leader.load()) {
        safe_print("[" + proc_name + "] Role: LEADER");
        safe_print("[" + proc_name + "] Total leader changes: " + to_string(final_changes));
        safe_print("[" + proc_name + "] FINAL_ROLE=LEADER");
        safe_print("[" + proc_name + "] FINAL_LEADER_CHANGES=" + to_string(final_changes));

        // Note: Each process can only report if IT became leader.
        // The shell script validates cluster-wide stability by counting
        // how many nodes report FINAL_ROLE=LEADER (should be exactly 1).
    } else {
        safe_print("[" + proc_name + "] Role: FOLLOWER");
        safe_print("[" + proc_name + "] FINAL_ROLE=FOLLOWER");
        safe_print("[" + proc_name + "] FINAL_LEADER_CHANGES=0");
    }

    // Always pass at the process level - individual processes cannot determine
    // cluster-wide stability. The shell script aggregates results from all nodes
    // to detect split-brain or leadership thrashing.
    bool pass = true;

    // Step 8: Cleanup
    safe_print("");
    safe_print("[" + proc_name + "] Step 6: Cleanup...");
    pre_shutdown_step();
    shutdown_paxos();

    safe_print("");
    safe_print("=========================================");
    if (pass) {
        safe_print("[" + proc_name + "] Test 1.5 PASSED ✓");
    } else {
        safe_print("[" + proc_name + "] Test 1.5 FAILED ✗");
    }
    safe_print("=========================================");

    return pass ? 0 : 1;
}
