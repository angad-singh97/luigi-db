#!/bin/bash

# testFiveServers: 5-Node Raft Cluster Test
#
# Purpose:
# - Validates Raft with 5 nodes (majority = 3)
# - Tests that leader waits for 3/5 ACKs before committing
# - Verifies consensus across all nodes

# CRITICAL: Clean up lingering processes from previous runs
echo "Pre-test cleanup: killing any lingering processes..."
pkill -9 -f 'build/testSingleLogSubmission' 2>/dev/null || true
sleep 2  # Give OS time to release ports

# Clean up old log files
rm -f five_server_*.log

echo "========================================="
echo "Test: 5-Server Raft Cluster"
echo "========================================="
echo "Testing majority quorum with 5 nodes (need 3/5 ACKs)"
echo ""

# Start all 5 servers SIMULTANEOUSLY
echo "Starting all 5 servers simultaneously..."

./build/testFiveServers p4 > five_server_p4.log 2>&1 &
P4_PID=$!

./build/testFiveServers p3 > five_server_p3.log 2>&1 &
P3_PID=$!

./build/testFiveServers p2 > five_server_p2.log 2>&1 &
P2_PID=$!

./build/testFiveServers p1 > five_server_p1.log 2>&1 &
P1_PID=$!

./build/testFiveServers localhost > five_server_localhost.log 2>&1 &
LOCALHOST_PID=$!

echo "  - Started p4 (PID=$P4_PID)"
echo "  - Started p3 (PID=$P3_PID)"
echo "  - Started p2 (PID=$P2_PID)"
echo "  - Started p1 (PID=$P1_PID)"
echo "  - Started localhost (PID=$LOCALHOST_PID)"

# Give them a moment to initialize
sleep 1

echo ""
echo "Waiting for test processes to complete..."

# Wait for all 5 processes to finish (with 30s timeout)
wait_time=0
max_wait=30
while kill -0 $P4_PID 2>/dev/null || kill -0 $P3_PID 2>/dev/null || kill -0 $P2_PID 2>/dev/null || kill -0 $P1_PID 2>/dev/null || kill -0 $LOCALHOST_PID 2>/dev/null; do
    if [ $wait_time -ge $max_wait ]; then
        echo "WARNING: Timeout after ${max_wait}s, forcefully terminating..."
        pkill -9 -f 'build/testFiveServers' 2>/dev/null || true
        sleep 2  # Give time for ports to release
        echo "ERROR: Test timed out - likely a connectivity or binding issue"
        echo "Check logs for port binding failures or connection timeouts"
        break
    fi
    sleep 1
    wait_time=$((wait_time + 1))
    if [ $((wait_time % 5)) -eq 0 ]; then
        echo "  Waited ${wait_time}s..."
    fi
done

echo "All processes completed after ${wait_time}s"
echo ""
echo "========================================="
echo "Test Results"
echo "========================================="

# Check each log
failed=0
nodes_with_log=0
declare -A node_contents

for log in five_server_localhost.log five_server_p1.log five_server_p2.log five_server_p3.log five_server_p4.log; do
    if [ -f "$log" ]; then
        node_name=$(echo "$log" | sed 's/five_server_//;s/.log//')

        echo ""
        echo "--- $node_name ---"

        # Check if node applied log
        if grep -q "Slot 1 applied: YES" "$log"; then
            nodes_with_log=$((nodes_with_log + 1))

            # Extract content
            content=$(grep "CONSENSUS_CHECK:" "$log" | sed 's/.*content=//')
            node_contents[$node_name]="$content"

            echo "✓ $node_name applied slot 1"
            echo "  Content: '$content'"
        else
            echo "✗ $node_name did NOT apply slot 1"
            failed=1
        fi

        # Check if test passed
        if grep -q "Test PASSED" "$log"; then
            echo "✓ $node_name: Test PASSED"
        else
            echo "✗ $node_name: Test FAILED"
        fi
    else
        echo "✗ $log: File not found"
        failed=1
    fi
done

echo ""
echo "========================================="
echo "Cluster Summary"
echo "========================================="
echo "Nodes with slot 1: $nodes_with_log / 5 (expected: 5)"

# Check consensus
if [ $nodes_with_log -eq 5 ]; then
    echo "✓ All nodes applied log"

    # Check if all contents are the same
    first_content="${node_contents[localhost]}"

    consensus=true
    for node in localhost p1 p2 p3 p4; do
        if [ "${node_contents[$node]}" != "$first_content" ]; then
            consensus=false
            echo "✗ Content mismatch: $node has '${node_contents[$node]}' but expected '$first_content'"
            failed=1
        fi
    done

    if [ "$consensus" = true ]; then
        echo "✓ CONSENSUS REACHED: All nodes have content '$first_content'"
    else
        echo "✗ CONSENSUS FAILED: Nodes have different content"
        failed=1
    fi
elif [ $nodes_with_log -eq 0 ]; then
    echo "✗ No nodes applied log (replication completely failed)"
    failed=1
else
    echo "⚠ Partial replication: Only $nodes_with_log / 5 nodes have the log"
    echo "  (This might be a timing issue or replication failure)"
    failed=1
fi

echo ""
echo "========================================="
echo "Protocol Verification (5-node quorum)"
echo "========================================="

# Find who was the leader
leader=$(grep -l "RAFT-PROTOCOL-1" five_server_*.log | sed 's/five_server_//;s/.log//' | head -1)

if [ -n "$leader" ]; then
    echo "Leader: $leader"

    # Count ACKs received
    ack_count=$(grep "RAFT-PROTOCOL-4" "five_server_${leader}.log" | wc -l)
    echo "ACKs received by leader: $ack_count / 4 followers"

    # Check majority calculation
    if grep -q "matchedIndices=\[0, 1, 1\]" "five_server_${leader}.log" || \
       grep -q "matchedIndices=\[0, 1, 1, 1\]" "five_server_${leader}.log" || \
       grep -q "matchedIndices=\[0, 1, 1, 1, 1\]" "five_server_${leader}.log"; then
        echo "✓ Majority quorum verified (need 3/5 nodes, median calculation correct)"
    fi

    # Show commit decision
    grep "RAFT-PROTOCOL-5" "five_server_${leader}.log" | head -1
fi

echo ""
echo "========================================="
if [ $failed -eq 0 ]; then
    echo "testFiveServers: PASSED ✓"
    echo "  - All 5 nodes applied the log"
    echo "  - Consensus achieved"
    echo "  - 5-node Raft cluster working correctly"
    exit_code=0
else
    echo "testFiveServers: FAILED ✗"
    exit_code=1
fi
echo "========================================="

echo ""
echo "Cleaning up processes..."
pkill -9 -f 'build/testFiveServers' 2>/dev/null || true
sleep 2  # Give OS time to release ports

exit $exit_code
