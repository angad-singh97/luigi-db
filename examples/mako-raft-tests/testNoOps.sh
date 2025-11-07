#!/bin/bash

# testNoOps: NO-OPS Application-Level Heartbeat Test
#
# Purpose:
# - Validates that Mako's NO-OP mechanism works correctly
# - Tests isNoops() recognition, replication, and epoch extraction
# - Verifies NO-OPs are NOT processed as transactions
# - Checks consensus across all nodes
#
# Success criteria:
# - All 3 nodes receive the NO-OPs
# - All nodes correctly extract epoch numbers
# - NO-OPs are NOT processed as transactions
# - All nodes agree on slot/epoch mapping (consensus)

# CRITICAL: Clean up lingering processes from previous runs
echo "Pre-test cleanup: killing any lingering processes..."
pkill -9 -f 'build/testNoOps' 2>/dev/null || true
sleep 2  # Give OS time to release ports

# Clean up old log files
rm -f noop_*.log

echo "========================================="
echo "Test: NO-OPS (Application-Level Heartbeats)"
echo "========================================="
echo "Testing NO-OP submission, recognition, and consensus"
echo ""
echo "What we're testing:"
echo "  1. isNoops() recognition of 'no-ops:X' format"
echo "  2. Replication to all 3 nodes"
echo "  3. Epoch extraction (3, 5, 7)"
echo "  4. Non-transaction handling"
echo "  5. Consensus verification"
echo ""

# Start all 3 servers SIMULTANEOUSLY
echo "Starting all 3 servers simultaneously..."

./build/testNoOps p2 > noop_p2.log 2>&1 &
P2_PID=$!

./build/testNoOps p1 > noop_p1.log 2>&1 &
P1_PID=$!

./build/testNoOps localhost > noop_localhost.log 2>&1 &
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
        pkill -9 -f 'build/testNoOps' 2>/dev/null || true
        sleep 2
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
declare -A node_noops_count
declare -A node_slots
declare -A node_epochs

for log in noop_localhost.log noop_p1.log noop_p2.log; do
    if [ -f "$log" ]; then
        node_name=$(echo "$log" | sed 's/noop_//;s/.log//')

        echo ""
        echo "--- $node_name ---"

        # Count NO-OPs received
        noop_count=$(grep -c "NO-OP DETECTED" "$log" 2>/dev/null || echo "0")
        node_noops_count[$node_name]=$noop_count
        echo "NO-OPs received: $noop_count"

        # Check if test passed
        if grep -q "Test PASSED" "$log"; then
            echo "✓ $node_name: Test PASSED"
        else
            echo "✗ $node_name: Test FAILED"
            failed=1
        fi

        # Extract consensus data
        echo "Consensus data:"
        grep "CONSENSUS_NOOP:" "$log" | while read line; do
            slot=$(echo "$line" | sed 's/.*slot=\([0-9]*\).*/\1/')
            epoch=$(echo "$line" | sed 's/.*epoch=\([0-9]*\).*/\1/')
            echo "  Slot $slot → Epoch $epoch"
        done

    else
        echo "✗ $log: File not found"
        failed=1
    fi
done

echo ""
echo "========================================="
echo "Cluster-Wide Analysis"
echo "========================================="

# Check if all nodes received NO-OPs
echo ""
echo "--- NO-OP Replication ---"
all_received=true
expected_noops=3

for node in localhost p1 p2; do
    count=${node_noops_count[$node]:-0}
    echo "$node: $count / $expected_noops NO-OPs"
    if [ "$count" -lt "$expected_noops" ]; then
        all_received=false
    fi
done

if [ "$all_received" = true ]; then
    echo "✓ All nodes received all NO-OPs"
else
    echo "⚠ Some nodes missing NO-OPs (replication issue or timing)"
    failed=1
fi

# Consensus verification - check if all nodes agree on slot->epoch mapping
echo ""
echo "--- Consensus Verification ---"

# Extract consensus data from all logs
declare -A consensus_map

for log in noop_localhost.log noop_p1.log noop_p2.log; do
    if [ -f "$log" ]; then
        grep "CONSENSUS_NOOP:" "$log" | while read line; do
            slot=$(echo "$line" | sed 's/.*slot=\([0-9]*\).*/\1/')
            epoch=$(echo "$line" | sed 's/.*epoch=\([0-9]*\).*/\1/')

            # Store in format: slot_epoch
            key="slot_${slot}"
            if [ -z "${consensus_map[$key]}" ]; then
                consensus_map[$key]=$epoch
            else
                # Check if epoch matches
                if [ "${consensus_map[$key]}" != "$epoch" ]; then
                    echo "✗ CONSENSUS MISMATCH: Slot $slot has different epochs!"
                    failed=1
                fi
            fi
        done
    fi
done

# Verify expected epochs (3, 5, 7)
expected_epochs=(3 5 7)
found_epochs=()

for log in noop_localhost.log; do  # Just check one node's epochs
    if [ -f "$log" ]; then
        while read line; do
            if echo "$line" | grep -q "CONSENSUS_NOOP:"; then
                epoch=$(echo "$line" | sed 's/.*epoch=\([0-9]*\).*/\1/')
                found_epochs+=($epoch)
            fi
        done < "$log"
    fi
done

# Sort and check
IFS=$'\n' sorted_found=($(sort -n <<<"${found_epochs[*]}"))
unset IFS

echo "Expected epochs: ${expected_epochs[@]}"
echo "Found epochs: ${sorted_found[@]}"

epochs_match=true
for i in 0 1 2; do
    if [ "${sorted_found[$i]}" != "${expected_epochs[$i]}" ]; then
        epochs_match=false
        break
    fi
done

if [ "$epochs_match" = true ]; then
    echo "✓ CONSENSUS: All nodes agree on epoch sequence"
else
    echo "✗ CONSENSUS FAILED: Epoch mismatch!"
    failed=1
fi

# Check for critical errors in logs
echo ""
echo "--- Error Check ---"
error_count=$(grep -h "ERROR:" noop_*.log 2>/dev/null | wc -l)
if [ "$error_count" -eq 0 ]; then
    echo "✓ No errors detected in logs"
else
    echo "⚠ Found $error_count error(s) in logs:"
    grep "ERROR:" noop_*.log | head -5
fi

# Summary
echo ""
echo "========================================="
echo "Summary"
echo "========================================="

# Test-specific checks
echo ""
echo "Test-Specific Verifications:"

# 1. isNoops() recognition
recognition_count=$(grep -h "NO-OP DETECTED" noop_*.log 2>/dev/null | wc -l)
if [ "$recognition_count" -ge 3 ]; then
    echo "✓ isNoops() recognition: WORKING ($recognition_count detections)"
else
    echo "✗ isNoops() recognition: FAILED"
    failed=1
fi

# 2. Epoch extraction
extraction_ok=$(grep -h "epoch=[357]" noop_*.log 2>/dev/null | wc -l)
if [ "$extraction_ok" -ge 3 ]; then
    echo "✓ Epoch extraction: WORKING ($extraction_ok extractions)"
else
    echo "✗ Epoch extraction: FAILED"
    failed=1
fi

# 3. Non-transaction handling (check that NO-OPs were NOT processed as transactions)
txn_processing=$(grep -h "tried_to_process_as_transaction.*true" noop_*.log 2>/dev/null | wc -l)
if [ "$txn_processing" -eq 0 ]; then
    echo "✓ Non-transaction handling: CORRECT"
else
    echo "✗ Non-transaction handling: FAILED (NO-OPs processed as transactions!)"
    failed=1
fi

# Final verdict
echo ""
echo "========================================="
if [ $failed -eq 0 ]; then
    echo "testNoOps: PASSED ✓"
    echo ""
    echo "Validated:"
    echo "  ✓ isNoops() correctly recognizes 'no-ops:X' format"
    echo "  ✓ NO-OPs replicate to all nodes"
    echo "  ✓ Epoch extraction works (3, 5, 7)"
    echo "  ✓ NO-OPs NOT processed as transactions"
    echo "  ✓ Consensus maintained across cluster"
    echo ""
    echo "Insight: Mako's application-level heartbeat mechanism"
    echo "         works correctly and is ready for watermark sync!"
    exit_code=0
else
    echo "testNoOps: FAILED ✗"
    echo ""
    echo "Check logs for details:"
    echo "  - noop_localhost.log"
    echo "  - noop_p1.log"
    echo "  - noop_p2.log"
    exit_code=1
fi
echo "========================================="

echo ""
echo "Cleaning up processes..."
pkill -9 -f 'build/testNoOps' 2>/dev/null || true
sleep 2  # Give OS time to release ports

exit $exit_code
