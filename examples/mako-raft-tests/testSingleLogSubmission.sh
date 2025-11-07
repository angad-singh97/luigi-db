#!/bin/bash

# testSingleLogSubmission: End-to-End Replication and Consensus Test
#
# Purpose:
# - Validates that a log is replicated to ALL nodes
# - Checks that all nodes reach consensus (same content at same slot)
# - Verifies end-to-end Raft replication pipeline
#
# Success criteria:
# - All 3 nodes apply a log at slot 1
# - All 3 nodes have the SAME content at slot 1
# - No crashes or segfaults

# CRITICAL: Clean up lingering processes from previous runs
echo "Pre-test cleanup: killing any lingering processes..."
# Kill only the binary (not this shell script!)
pkill -9 -f 'build/testSingleLogSubmission' 2>/dev/null || true
sleep 2  # Give OS time to release ports

# Clean up old log files
rm -f single_log_*.log

echo "========================================="
echo "Test: Single Log Replication (End-to-End)"
echo "========================================="
echo "Testing log submission and consensus verification"
echo ""

# Start all 3 servers SIMULTANEOUSLY
echo "Starting all 3 servers simultaneously..."

./build/testSingleLogSubmission p2 > single_log_p2.log 2>&1 &
P2_PID=$!

./build/testSingleLogSubmission p1 > single_log_p1.log 2>&1 &
P1_PID=$!

./build/testSingleLogSubmission localhost > single_log_localhost.log 2>&1 &
LOCALHOST_PID=$!

echo "  - Started p2 (PID=$P2_PID)"
echo "  - Started p1 (PID=$P1_PID)"
echo "  - Started localhost (PID=$LOCALHOST_PID)"

# Give them a moment to initialize
sleep 1

echo ""
echo "Waiting for test processes to complete..."

# Wait for all 3 processes to finish (with 30s timeout - increased from 20s)
wait_time=0
max_wait=30
while kill -0 $P2_PID 2>/dev/null || kill -0 $P1_PID 2>/dev/null || kill -0 $LOCALHOST_PID 2>/dev/null; do
    if [ $wait_time -ge $max_wait ]; then
        echo "WARNING: Timeout after ${max_wait}s, forcefully terminating..."
        pkill -9 -f 'build/testSingleLogSubmission' 2>/dev/null || true
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

for log in single_log_localhost.log single_log_p1.log single_log_p2.log; do
    if [ -f "$log" ]; then
        node_name=$(echo "$log" | sed 's/single_log_//;s/.log//')

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
echo "Nodes with slot 1: $nodes_with_log / 3 (expected: 3)"

# Check consensus
if [ $nodes_with_log -eq 3 ]; then
    echo "✓ All nodes applied log"

    # Check if all contents are the same
    contents_array=("${node_contents[@]}")
    first_content="${node_contents[localhost]}"

    consensus=true
    for node in localhost p1 p2; do
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
    echo "⚠ Partial replication: Only $nodes_with_log / 3 nodes have the log"
    echo "  (This might be a timing issue or replication failure)"
    failed=1
fi

echo ""
echo "========================================="
if [ $failed -eq 0 ]; then
    echo "testSingleLogSubmission: PASSED ✓"
    echo "  - All nodes applied the log"
    echo "  - Consensus achieved"
    echo "  - Replication working correctly"
    exit_code=0
else
    echo "testSingleLogSubmission: FAILED ✗"
    exit_code=1
fi
echo "========================================="

echo ""
echo "Cleaning up processes..."
pkill -9 -f 'build/testSingleLogSubmission' 2>/dev/null || true
sleep 2  # Give OS time to release ports

exit $exit_code
