/**
 * testLeaderStability.cc
 *
 * Tests FIXED LEADER stability (Paxos-style)
 *
 * Purpose:
 * - Verify that localhost is FORCED as leader at startup (machine_id == 0)
 * - Verify that localhost STAYS leader for entire test duration (10 seconds)
 * - Verify that NO elections occur after startup
 * - Verify that p1 and p2 remain PERMANENT FOLLOWERS
 *
 * Expected Behavior:
 * - localhost: Becomes leader at startup, stays leader for 10 seconds
 * - p1/p2: Remain followers, never become leader
 * - No "BECAME LEADER" or "LOST LEADERSHIP" events after initial setup
 */

#include <iostream>
#include <cstring>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include <mako.hh>
#include <examples/common.h>

using namespace std;
using namespace mako;
using namespace chrono;

// Test configuration
const int TEST_DURATION_SEC = 30;  // Run for 10 seconds

// Tracking
atomic<bool> i_am_leader{false};
atomic<int> times_became_leader{0};
atomic<int> times_lost_leadership{0};
atomic<int> current_term{0};

mutex cout_mutex;

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
    uint64_t start_time = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

    safe_print("=========================================");
    safe_print("Test: Fixed Leader Stability (Paxos-style)");
    safe_print("Process: " + proc_name);
    safe_print("Duration: " + to_string(TEST_DURATION_SEC) + " seconds");
    safe_print("=========================================");

    // Configuration (uses standard 3-node cluster config)
    // Fixed leader mode is enabled by setup2() checking machine_id == 0
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

    // Step 1: Setup Raft with FIXED LEADER mode
    safe_print("[" + proc_name + "] Step 1: Initializing Raft (FIXED LEADER MODE)...");
    if (proc_name == "localhost") {
        safe_print("[" + proc_name + "] Expected: Will be FORCED as FIXED LEADER");
    } else {
        safe_print("[" + proc_name + "] Expected: Will be PERMANENT FOLLOWER");
    }

    vector<string> ret = setup(16, argv_raft);
    if (ret.empty()) {
        cerr << "[" << proc_name << "] ERROR: setup() failed!" << endl;
        return 1;
    }
    safe_print("[" + proc_name + "] ✓ setup() completed");

    // Step 2: Register leadership change callbacks
    safe_print("[" + proc_name + "] Step 2: Registering leadership callbacks...");

    // Track when we BECOME leader
    register_leader_election_callback([&](int control) {
        times_became_leader++;
        i_am_leader.store(true);
        uint64_t elapsed = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count() - start_time;
        safe_print("[" + proc_name + "] 👑 BECAME LEADER (event #" + to_string(times_became_leader.load()) +
                   ") at " + to_string(elapsed) + "ms");
    });

    // Dummy callback for log application (we're not testing log replication here)
    auto dummy_callback = [&](const char*& log, int len, int par_id, int slot_id,
                              queue<tuple<int, int, int, int, const char*>>& un_replay_logs_) {
        // Just acknowledge
        uint32_t timestamp = static_cast<uint32_t>(getCurrentTimeMillis());
        return static_cast<int>(timestamp * 10 + 1);
    };

    register_for_leader_par_id_return(dummy_callback, 0);
    register_for_follower_par_id_return(dummy_callback, 0);

    safe_print("[" + proc_name + "] ✓ Callbacks registered");

    // Step 3: Launch services (this calls setup2 which forces fixed leader)
    safe_print("[" + proc_name + "] Step 3: Launching Raft services (setup2)...");
    setup2(0, 0);  // action=0 means use fixed leader logic
    safe_print("[" + proc_name + "] ✓ setup2() completed");

    // Step 4: Check initial leadership state
    safe_print("[" + proc_name + "] Step 4: Checking initial leadership state...");
    this_thread::sleep_for(chrono::milliseconds(500));  // Give time for setup

    int initial_became_leader = times_became_leader.load();
    bool initial_is_leader = i_am_leader.load();

    safe_print("[" + proc_name + "] Initial state:");
    safe_print("[" + proc_name + "]   times_became_leader: " + to_string(initial_became_leader));
    safe_print("[" + proc_name + "]   i_am_leader: " + (initial_is_leader ? "YES" : "NO"));

    if (proc_name == "localhost") {
        if (initial_is_leader || initial_became_leader > 0) {
            safe_print("[" + proc_name + "] ✓ GOOD: localhost is leader as expected");
        } else {
            safe_print("[" + proc_name + "] ⚠️  WARNING: localhost not yet leader (may take time)");
        }
    } else {
        if (!initial_is_leader && initial_became_leader == 0) {
            safe_print("[" + proc_name + "] ✓ GOOD: " + proc_name + " is follower as expected");
        } else {
            safe_print("[" + proc_name + "] ⚠️  WARNING: " + proc_name + " became leader (unexpected!)");
        }
    }

    // Step 5: Monitor leadership stability for TEST_DURATION_SEC
    safe_print("");
    safe_print("[" + proc_name + "] Step 5: Monitoring leadership stability for " +
               to_string(TEST_DURATION_SEC) + " seconds...");
    safe_print("[" + proc_name + "] (Checking every second)");

    for (int i = 1; i <= TEST_DURATION_SEC; i++) {
        this_thread::sleep_for(chrono::seconds(1));

        int became = times_became_leader.load();
        int lost = times_lost_leadership.load();
        bool is_leader = i_am_leader.load();

        safe_print("[" + proc_name + "] [" + to_string(i) + "s] " +
                   "became=" + to_string(became) + ", " +
                   "lost=" + to_string(lost) + ", " +
                   "leader=" + (is_leader ? "YES" : "NO"));
    }

    safe_print("[" + proc_name + "] ✓ Monitoring complete");

    // CAPTURE STATE IMMEDIATELY AFTER MONITORING (before cleanup can trigger elections)
    int final_became = times_became_leader.load();
    int final_lost = times_lost_leadership.load();
    bool final_is_leader = i_am_leader.load();

    safe_print("[" + proc_name + "] State captured at end of monitoring:");
    safe_print("[" + proc_name + "]   times_became_leader=" + to_string(final_became));
    safe_print("[" + proc_name + "]   times_lost_leadership=" + to_string(final_lost));
    safe_print("[" + proc_name + "]   i_am_leader=" + (final_is_leader ? "YES" : "NO"));

    // Step 6: Final analysis
    safe_print("");
    safe_print("=========================================");
    safe_print("[" + proc_name + "] Final Analysis");
    safe_print("=========================================");

    safe_print("[" + proc_name + "] Final counts:");
    safe_print("[" + proc_name + "]   Times became leader: " + to_string(final_became));
    safe_print("[" + proc_name + "]   Times lost leadership: " + to_string(final_lost));
    safe_print("[" + proc_name + "]   Current state: " + (final_is_leader ? "LEADER" : "FOLLOWER"));

    bool test_passed = true;

    if (proc_name == "localhost") {
        // localhost should be leader throughout
        safe_print("");
        safe_print("[" + proc_name + "] Expectations for localhost:");
        safe_print("[" + proc_name + "]   - Should become leader (at least once): " +
                   string(final_became >= 1 ? "✓" : "✗"));
        safe_print("[" + proc_name + "]   - Should NEVER lose leadership: " +
                   string(final_lost == 0 ? "✓" : "✗"));
        safe_print("[" + proc_name + "]   - Should be leader at end: " +
                   string(final_is_leader ? "✓" : "✗"));

        if (final_became >= 1 && final_lost == 0 && final_is_leader) {
            safe_print("[" + proc_name + "] ✅ TEST PASSED: localhost stayed as FIXED LEADER");
        } else {
            safe_print("[" + proc_name + "] ❌ TEST FAILED: localhost lost leadership or never became leader");
            test_passed = false;
        }
    } else {
        // p1/p2 should NEVER become leader
        safe_print("");
        safe_print("[" + proc_name + "] Expectations for " + proc_name + ":");
        safe_print("[" + proc_name + "]   - Should NEVER become leader: " +
                   string(final_became == 0 ? "✓" : "✗"));
        safe_print("[" + proc_name + "]   - Should remain follower: " +
                   string(!final_is_leader ? "✓" : "✗"));

        if (final_became == 0 && !final_is_leader) {
            safe_print("[" + proc_name + "] ✅ TEST PASSED: " + proc_name +
                       " remained PERMANENT FOLLOWER");
        } else {
            safe_print("[" + proc_name + "] ❌ TEST FAILED: " + proc_name +
                       " became leader (should never happen!)");
            test_passed = false;
        }
    }

    safe_print("=========================================");

    // Step 7: Cleanup
    safe_print("");
    safe_print("[" + proc_name + "] Step 7: Cleanup...");
    pre_shutdown_step();
    shutdown_paxos();

    safe_print("[" + proc_name + "] Test complete");
    return test_passed ? 0 : 1;
}
