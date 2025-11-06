#include <iostream>
#include <cstring>
#include <vector>
#include <mako.hh>
#include <examples/common.h>

using namespace std;
using namespace mako;

int main(int argc, char **argv) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <process_name>" << endl;
        cerr << "  process_name: localhost, p1, or p2" << endl;
        return 1;
    }

    string proc_name = std::string(argv[1]);

    cout << "=========================================" << endl;
    cout << "Basic Setup Test for: " << proc_name << endl;
    cout << "=========================================" << endl;

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

    cout << "[" << proc_name << "] Step 1: Calling setup()..." << endl;
    std::vector<string> ret = setup(16, argv_raft);
    if (ret.empty()) {
        cerr << "[" << proc_name << "] ERROR: setup() failed!" << endl;
        return 1;
    }
    cout << "[" << proc_name << "] ✓ setup() completed successfully" << endl;

    cout << "[" << proc_name << "] Step 2: Calling setup2()..." << endl;
    setup2(0, 0);
    cout << "[" << proc_name << "] ✓ setup2() completed successfully" << endl;

    cout << "[" << proc_name << "] Step 3: Waiting 5 seconds for system to stabilize..." << endl;
    sleep(5);

    cout << "[" << proc_name << "] Step 4: Cleanup..." << endl;
    pre_shutdown_step();
    shutdown_paxos();

    cout << endl;
    cout << "=========================================" << endl;
    cout << "[" << proc_name << "] Basic Setup Test PASSED ✓" << endl;
    cout << "=========================================" << endl;

    return 0;
}
