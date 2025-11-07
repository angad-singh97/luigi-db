#!/bin/bash

# testLeaderStability.sh
#
# Tests FIXED LEADER stability (Paxos-style)
#
# Expected:
# - localhost: Forced as leader at startup, stays leader for 10 seconds
# - p1/p2: Permanent followers, never become leader

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_header() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}========================================${NC}"
}

print_success() {
    echo -e "${GREEN}✅ $1${NC}"
}

print_error() {
    echo -e "${RED}❌ $1${NC}"
}

cleanup() {
    echo ""
    print_header "Cleaning up..."
    kill $P2_PID 2>/dev/null || true
    kill $P1_PID 2>/dev/null || true
    kill $LOCALHOST_PID 2>/dev/null || true
    sleep 1
    pkill -9 testLeaderStability 2>/dev/null || true
    echo "Cleanup complete"
}

trap cleanup EXIT

print_header "Fixed Leader Stability Test (Paxos-style)"

# Step 1: Check binary
if [ ! -f "./build/testLeaderStability" ]; then
    print_error "testLeaderStability binary not found"
    echo "Please run 'make' to build the project"
    exit 1
fi
print_success "Binary found"

# Step 2: Clean old logs
rm -f stability_*.log
print_success "Old logs cleaned"

# Step 3: Start all three nodes
print_header "Starting 3-node cluster"

echo "Starting p2 (should be PERMANENT FOLLOWER)..."
./build/testLeaderStability p2 > stability_p2.log 2>&1 &
P2_PID=$!
echo "  PID: $P2_PID"
sleep 1

echo "Starting p1 (should be PERMANENT FOLLOWER)..."
./build/testLeaderStability p1 > stability_p1.log 2>&1 &
P1_PID=$!
echo "  PID: $P1_PID"
sleep 1

echo "Starting localhost (should be FIXED LEADER)..."
./build/testLeaderStability localhost > stability_localhost.log 2>&1 &
LOCALHOST_PID=$!
echo "  PID: $LOCALHOST_PID"
sleep 1

print_success "All nodes started"

# Step 4: Wait for test to complete (10 seconds + buffer)
print_header "Waiting for test completion"
echo "Test runs for 10 seconds..."
sleep 15
print_success "Test complete"

# Step 5: Wait for processes
wait $LOCALHOST_PID 2>/dev/null || true
wait $P1_PID 2>/dev/null || true
wait $P2_PID 2>/dev/null || true

# Step 6: Analysis
print_header "Analyzing Results"

if [ ! -f "stability_localhost.log" ] || [ ! -f "stability_p1.log" ] || [ ! -f "stability_p2.log" ]; then
    print_error "One or more log files missing"
    exit 1
fi

echo ""
echo "=== localhost (should be FIXED LEADER) ==="
localhost_became=$(grep -c "BECAME LEADER" stability_localhost.log 2>/dev/null || true)
localhost_became=${localhost_became:-0}
localhost_passed=$(grep -c "TEST PASSED: localhost stayed as FIXED LEADER" stability_localhost.log 2>/dev/null || true)
localhost_passed=${localhost_passed:-0}
echo "Times became leader: $localhost_became"
echo "Test status: $(grep 'TEST PASSED\|TEST FAILED' stability_localhost.log 2>/dev/null | tail -1)"

echo ""
echo "=== p1 (should be PERMANENT FOLLOWER) ==="
p1_became=$(grep -c "BECAME LEADER" stability_p1.log 2>/dev/null || true)
p1_became=${p1_became:-0}
p1_passed=$(grep -c "TEST PASSED.*remained PERMANENT FOLLOWER" stability_p1.log 2>/dev/null || true)
p1_passed=${p1_passed:-0}
echo "Times became leader: $p1_became"
echo "Test status: $(grep 'TEST PASSED\|TEST FAILED' stability_p1.log 2>/dev/null | tail -1)"

echo ""
echo "=== p2 (should be PERMANENT FOLLOWER) ==="
p2_became=$(grep -c "BECAME LEADER" stability_p2.log 2>/dev/null || true)
p2_became=${p2_became:-0}
p2_passed=$(grep -c "TEST PASSED.*remained PERMANENT FOLLOWER" stability_p2.log 2>/dev/null || true)
p2_passed=${p2_passed:-0}
echo "Times became leader: $p2_became"
echo "Test status: $(grep 'TEST PASSED\|TEST FAILED' stability_p2.log 2>/dev/null | tail -1)"

# Final verdict
echo ""
print_header "FINAL VERDICT"

all_passed=true

# Check localhost
if [ "$localhost_became" -ge 1 ] && [ "$localhost_passed" -ge 1 ]; then
    print_success "localhost: Became leader and stayed stable ✓"
else
    print_error "localhost: Failed to stay as fixed leader ✗"
    all_passed=false
fi

# Check p1
if [ "$p1_became" -eq 0 ] && [ "$p1_passed" -ge 1 ]; then
    print_success "p1: Remained permanent follower ✓"
else
    print_error "p1: Unexpectedly became leader ✗"
    all_passed=false
fi

# Check p2
if [ "$p2_became" -eq 0 ] && [ "$p2_passed" -ge 1 ]; then
    print_success "p2: Remained permanent follower ✓"
else
    print_error "p2: Unexpectedly became leader ✗"
    all_passed=false
fi

echo ""
if [ "$all_passed" = true ]; then
    print_success "✅ ALL TESTS PASSED"
    echo ""
    echo "Summary:"
    echo "  - localhost was FIXED LEADER (like Paxos)"
    echo "  - p1 and p2 were PERMANENT FOLLOWERS"
    echo "  - No elections occurred"
    echo "  - Leadership remained stable for 10 seconds"
    exit 0
else
    print_error "❌ SOME TESTS FAILED"
    echo ""
    echo "Check logs for details:"
    echo "  - stability_localhost.log"
    echo "  - stability_p1.log"
    echo "  - stability_p2.log"
    exit 1
fi
