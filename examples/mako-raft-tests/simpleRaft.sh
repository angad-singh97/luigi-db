#!/bin/bash

# simpleRaft.sh - Basic Raft replication test
# Tests 3 Raft replicas: localhost (preferred leader), p1, p2
# Note: Unlike Paxos which has a separate "learner", Raft treats all replicas equally

rm -f raft_a1.log raft_a2.log raft_a3.log

# Start the 3 Raft replicas (same order as simplePaxos for consistency)
nohup ./build/simpleRaft localhost > raft_a1.log 2>&1 &
sleep 1
nohup ./build/simpleRaft p1 > raft_a2.log 2>&1 &
sleep 1
nohup ./build/simpleRaft p2 > raft_a3.log 2>&1 &

# Wait for test to complete
# - 5s for leader election and transfer
# - ~1s for log submission (50 logs * 10ms = 500ms)
# - 2s for replication
# - 2s for shutdown
# Total: ~10s, use 15s to be safe
sleep 15

tail -n 5 raft_a1.log raft_a2.log raft_a3.log

# Check if all log files contain PASS keyword
echo ""
echo "Checking test results..."
failed=0
for log in raft_a1.log raft_a2.log raft_a3.log; do
    if [ -f "$log" ]; then
        if grep -q "PASS" "$log"; then
            echo "$log: PASS found ✓"
        else
            echo "$log: PASS not found ✗"
            failed=1
        fi
    else
        echo "$log: File not found ✗"
        failed=1
    fi
done

if [ $failed -eq 1 ]; then
    echo "Check failed - not all files contain PASS"
    exit 1
else
    echo "All checks passed!"
fi
