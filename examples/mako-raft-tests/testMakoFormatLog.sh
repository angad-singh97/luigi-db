#!/bin/bash

# testMakoFormatLog: Test Mako Transaction Format Replication
#
# Purpose:
# - Validates that Raft can replicate REAL Mako transaction logs
# - Tests binary format: [commit_id][count][len][K-V pairs...][final_commit_id][latency]
# - Verifies all nodes can decode and replay the Mako format
#
# Success criteria:
# - All 3 nodes apply a Mako-format log at slot 1
# - All 3 nodes correctly decode the transaction format
# - K-V pairs are extracted correctly

# CRITICAL: Clean up lingering processes from previous runs
echo "Pre-test cleanup: killing any lingering processes..."
pkill -9 -f 'build/testMakoFormatLog' 2>/dev/null || true
sleep 2  # Give OS time to release ports

# Clean up old log files
rm -f mako_format_*.log

echo "========================================="
echo "Test: Mako Format Log Replication"
echo "========================================="
echo "Testing real Mako transaction format replication"
echo ""

# Start all 3 servers SIMULTANEOUSLY
echo "Starting all 3 servers simultaneously..."

./build/testMakoFormatLog p2 > mako_format_p2.log 2>&1 &
P2_PID=$!

./build/testMakoFormatLog p1 > mako_format_p1.log 2>&1 &
P1_PID=$!

./build/testMakoFormatLog localhost > mako_format_localhost.log 2>&1 &
LOCALHOST_PID=$!

echo "  - Started p2 (PID=$P2_PID)"
echo "  - Started p1 (PID=$P1_PID)"
echo "  - Started localhost (PID=$LOCALHOST_PID)"

# Give them a moment to initialize
sleep 1

echo ""
echo "Waiting for test processes to complete..."

# Wait for all 3 processes to finish (with 30s timeout)
wait_time=0
max_wait=30
while kill -0 $P2_PID 2>/dev/null || kill -0 $P1_PID 2>/dev/null || kill -0 $LOCALHOST_PID 2>/dev/null; do
    if [ $wait_time -ge $max_wait ]; then
        echo "WARNING: Timeout after ${max_wait}s, forcefully terminating..."
        pkill -9 -f 'build/testMakoFormatLog' 2>/dev/null || true
        sleep 2
        echo "ERROR: Test timed out"
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
declare -A node_decoded_count

for log in mako_format_localhost.log mako_format_p1.log mako_format_p2.log; do
    if [ -f "$log" ]; then
        node_name=$(echo "$log" | sed 's/mako_format_//;s/.log//')

        echo ""
        echo "--- $node_name ---"

        # Check if node applied log
        if grep -q "Slot 1 applied: YES" "$log"; then
            nodes_with_log=$((nodes_with_log + 1))

            echo "✓ $node_name applied slot 1"

            # Check if node decoded the Mako format
            if grep -q "Decoded transaction:" "$log"; then
                echo "✓ $node_name decoded Mako format successfully"

                # Extract decoded K-V pairs count
                decoded_count=$(grep -oP 'K-V pairs: \K\d+' "$log" | head -1)
                node_decoded_count[$node_name]=$decoded_count

                echo "  K-V pairs decoded: $decoded_count"

                # Show the decoded K-V pairs
                grep "\[0\]" "$log" | head -2 | sed 's/^/    /'
            else
                echo "✗ $node_name failed to decode Mako format"
                failed=1
            fi

            # Check validation
            if grep -q "All validation checks passed" "$log"; then
                echo "✓ $node_name validation PASSED"
            else
                echo "✗ $node_name validation FAILED"
                failed=1
            fi
        else
            echo "✗ $node_name did NOT apply slot 1"
            failed=1
        fi

        # Check if test passed overall
        if grep -q "Test PASSED" "$log"; then
            echo "✓ $node_name: Test PASSED"
        else
            echo "✗ $node_name: Test FAILED"
            failed=1
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

# Check if all nodes decoded the same thing
if [ $nodes_with_log -eq 3 ]; then
    echo "✓ All nodes applied log"

    # Check consensus on decoded data
    first_count="${node_decoded_count[localhost]}"
    consensus=true

    for node in localhost p1 p2; do
        if [ "${node_decoded_count[$node]}" != "$first_count" ]; then
            consensus=false
            echo "✗ Decode mismatch: $node decoded ${node_decoded_count[$node]} but expected $first_count"
            failed=1
        fi
    done

    if [ "$consensus" = true ]; then
        echo "✓ CONSENSUS REACHED: All nodes decoded $first_count K-V pairs"
    else
        echo "✗ CONSENSUS FAILED: Nodes decoded different data"
        failed=1
    fi
elif [ $nodes_with_log -eq 0 ]; then
    echo "✗ No nodes applied log (replication completely failed)"
    failed=1
else
    echo "⚠ Partial replication: Only $nodes_with_log / 3 nodes have the log"
    failed=1
fi

echo ""
echo "========================================="
if [ $failed -eq 0 ]; then
    echo "testMakoFormatLog: PASSED ✓"
    echo "  - All nodes applied the Mako-format log"
    echo "  - All nodes decoded the format correctly"
    echo "  - Consensus achieved on decoded data"
    echo "  - Raft supports Mako transaction format!"
    exit_code=0
else
    echo "testMakoFormatLog: FAILED ✗"
    exit_code=1
fi
echo "========================================="

echo ""
echo "Cleaning up processes..."
pkill -9 -f 'build/testMakoFormatLog' 2>/dev/null || true
sleep 2

exit $exit_code
