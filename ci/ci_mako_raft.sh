#!/bin/bash

################################################################################
# ci_mako_raft.sh - Continuous Integration Script for Mako-Raft Integration
################################################################################
# This script mirrors ci.sh but tests Raft instead of Paxos
################################################################################

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

################################################################################
# Cleanup Function
################################################################################

cleanup_processes() {
    echo "Cleaning up any lingering test processes..."
    pkill -9 -f simpleTransaction 2>/dev/null || true
    pkill -9 -f simpleRaft 2>/dev/null || true
    pkill -9 -f testPreferredReplicaStartup 2>/dev/null || true
    pkill -9 -f testPreferredReplicaLogReplication 2>/dev/null || true
    pkill -9 -f testNoOps 2>/dev/null || true
    pkill -9 -f dbtest 2>/dev/null || true
    sleep 1  # Give OS time to release ports
    echo "Cleanup complete."
}

################################################################################
# Test Functions
################################################################################

# Function 1: Compile
compile() {
    make -j32
}

# Function 2: Run simple transaction test (no replication)
run_simple_transaction() {
    cleanup_processes
    ./build/simpleTransaction
}

# Function 3: Run simple Raft test (basic replication)
run_simple_raft() {
    cleanup_processes
    bash ./examples/mako-raft-tests/simpleRaft.sh
}

# Function 4: Run 2-shard Raft test (no replication)
run_2shard_no_replication_raft() {
    cleanup_processes
    bash ./examples/mako-raft-tests/test_2shard_no_replication_raft.sh
}

# Function 5: Run 1-shard Raft test (with replication)
run_1shard_replication_raft() {
    cleanup_processes
    bash ./examples/mako-raft-tests/test_1shard_replication_raft.sh
}

# Function 6: Run 2-shard Raft test (with replication)
run_2shard_replication_raft() {
    cleanup_processes
    bash ./examples/mako-raft-tests/test_2shard_replication_raft.sh
}

################################################################################
# Main Entry Point
################################################################################

case "${1:-}" in
    compile)
        compile
        ;;
    simpleTransaction)
        run_simple_transaction
        ;;
    simpleRaft)
        run_simple_raft
        ;;
    shardNoReplicationRaft)
        run_2shard_no_replication_raft
        ;;
    shard1ReplicationRaft)
        run_1shard_replication_raft
        ;;
    shard2ReplicationRaft)
        run_2shard_replication_raft
        ;;
    all)
        # Run all steps in sequence
        compile
        run_simple_transaction
        run_simple_raft
        run_2shard_no_replication_raft
        run_1shard_replication_raft
        run_2shard_replication_raft
        echo "All CI steps completed successfully!"
        ;;
    *)
        echo "Usage: $0 {compile|simpleTransaction|simpleRaft|shardNoReplicationRaft|shard1ReplicationRaft|shard2ReplicationRaft|all}"
        echo ""
        echo "Commands:"
        echo "  compile                  - Build Mako with Raft support"
        echo "  simpleTransaction        - Run simple transaction test (no replication)"
        echo "  simpleRaft               - Run simple Raft replication test"
        echo "  shardNoReplicationRaft   - Run 2-shard Raft test (no replication)"
        echo "  shard1ReplicationRaft    - Run 1-shard Raft test (with replication)"
        echo "  shard2ReplicationRaft    - Run 2-shard Raft test (with replication)"
        echo "  all                      - Run all tests"
        exit 1
        ;;
esac
