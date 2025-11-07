#!/bin/bash

# testElectionStability: Leader Election Stability Test
# Purpose: Validate that leader election is stable without log submission

# Clean up old log files
rm -f election_stability_*.log

echo "========================================="
echo "Test: Election Stability"
echo "========================================="
echo "Testing stable leader election (no log submission)"
echo ""

# IMPROVEMENT: Start all servers simultaneously to reduce timing issues
echo "Starting all 3 servers simultaneously..."

# Use background jobs and wait for all to start
./build/testElectionStability p2 > election_stability_p2.log 2>&1 &
P2_PID=$!

./build/testElectionStability p1 > election_stability_p1.log 2>&1 &
P1_PID=$!

./build/testElectionStability localhost > election_stability_localhost.log 2>&1 &
LOCALHOST_PID=$!

echo "  - Started p2 (PID=$P2_PID)"
echo "  - Started p1 (PID=$P1_PID)"
echo "  - Started localhost (PID=$LOCALHOST_PID)"

# Give them a moment to initialize
sleep 1

echo ""
echo "All servers started. Waiting for test to complete (12 seconds)..."
echo "  T+0s : Servers starting up"
sleep 3
echo "  T+3s : Initial election should complete"
sleep 2
echo "  T+5s : Monitoring stability..."
sleep 5
echo "  T+10s: Final check"
sleep 2

echo ""
echo "========================================="
echo "Test Results"
echo "========================================="

# Check each log
failed=0
leader_count=0

for log in election_stability_localhost.log election_stability_p1.log election_stability_p2.log; do
    if [ -f "$log" ]; then
        echo ""
        echo "--- $log ---"

        # Show last 25 lines for more context
        tail -25 "$log"

        if grep -q "Test 1.5 PASSED" "$log"; then
            echo "✓ $log: PASSED"

            # Count leaders
            final_role=$(grep "FINAL_ROLE=" "$log" | tail -1 | cut -d'=' -f2)
            final_changes=$(grep "FINAL_LEADER_CHANGES=" "$log" | tail -1 | cut -d'=' -f2)

            if [ "$final_role" = "LEADER" ]; then
                leader_count=$((leader_count + 1))
            fi

            if [ -n "$final_changes" ]; then
                echo "  Leader changes: $final_changes"
                if [ "$final_role" = "LEADER" ] && [ "$final_changes" -gt 3 ]; then
                    echo "  ⚠ Warning: Excessive leader changes detected"
                fi
            fi
        else
            echo "✗ $log: FAILED"
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
echo "Number of leaders: $leader_count (expected: 1)"

if [ $leader_count -eq 1 ]; then
    echo "✓ Exactly one leader elected"
elif [ $leader_count -eq 0 ]; then
    echo "✗ No leader elected (election failed)"
    failed=1
else
    echo "✗ Multiple leaders elected (split-brain detected!)"
    failed=1
fi

echo ""
echo "========================================="
if [ $failed -eq 0 ]; then
    echo "testElectionStability: PASSED ✓"
    exit_code=0
else
    echo "testElectionStability: FAILED ✗"
    exit_code=1
fi
echo "========================================="

echo ""
echo "Cleaning up processes..."
pkill -f testElectionStability || true
sleep 1

exit $exit_code
