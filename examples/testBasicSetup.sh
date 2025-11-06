#!/bin/bash

# Clean up old log files
rm -f basic_setup_*.log

echo "========================================="
echo "Basic Setup Test"
echo "========================================="
echo "Testing if all 3 nodes can start up properly"
echo ""

# Start all 3 servers in reverse order
echo "Starting server: p2..."
./build/testBasicSetup p2 > basic_setup_p2.log 2>&1 &
P2_PID=$!
sleep 2

echo "Starting server: p1..."
./build/testBasicSetup p1 > basic_setup_p1.log 2>&1 &
P1_PID=$!
sleep 2

echo "Starting server: localhost..."
./build/testBasicSetup localhost > basic_setup_localhost.log 2>&1 &
LOCALHOST_PID=$!
sleep 2

echo ""
echo "All servers started. Waiting for test to complete (10 seconds)..."
sleep 10

echo ""
echo "========================================="
echo "Test Results"
echo "========================================="

# Check each log
failed=0

for log in basic_setup_localhost.log basic_setup_p1.log basic_setup_p2.log; do
    if [ -f "$log" ]; then
        echo ""
        echo "--- $log ---"
        tail -10 "$log"

        if grep -q "Basic Setup Test PASSED" "$log"; then
            echo "✓ $log: PASSED"
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
if [ $failed -eq 0 ]; then
    echo "All Basic Setup Tests PASSED ✓"
    exit_code=0
else
    echo "Basic Setup Test FAILED ✗"
    exit_code=1
fi
echo "========================================="

echo ""
echo "Cleaning up processes..."
pkill -f testBasicSetup || true
sleep 1

exit $exit_code
