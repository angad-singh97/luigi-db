#!/bin/bash
# Network latency tests for 1shard-3replicas with different geographic scenarios
set -e

cd /root/cse532/mako

# Test configuration
CONFIG="src/deptran/luigi/test/configs/1shard-3replicas.yml"
DURATION=10
RESULTS_DIR="src/deptran/luigi/test/results/network_latency"

mkdir -p "$RESULTS_DIR"

echo "=== Network Latency Testing for 1-Shard-3-Replicas ==="
echo ""

# Function to run a single test
run_test() {
    local network_name=$1
    local delay=$2
    local jitter=$3
    local owd=$4
    local headroom=$5
    local threads=$6

    echo "=========================================="
    echo "Testing: $network_name - $threads threads"
    echo "Network: ${delay}ms ± ${jitter}ms"
    echo "OWD: ${owd}ms, Headroom: ${headroom}ms"
    echo "=========================================="

    # Kill any existing processes
    pkill -9 luigi_server 2>/dev/null || true
    pkill -9 luigi_coordinator 2>/dev/null || true
    sleep 1

    # Clear any existing network emulation
    tc qdisc del dev lo root 2>/dev/null || true

    # Apply network emulation
    tc qdisc add dev lo root netem delay ${delay}ms ${jitter}ms distribution pareto
    sleep 1

    # Start replicas
    ./build/luigi_server -f "$CONFIG" -P s101 > s101_micro.log 2>&1 &
    S1_PID=$!
    sleep 2

    ./build/luigi_server -f "$CONFIG" -P s102 > s102_micro.log 2>&1 &
    S2_PID=$!
    sleep 2

    ./build/luigi_server -f "$CONFIG" -P s103 > s103_micro.log 2>&1 &
    S3_PID=$!
    sleep 3

    # Run coordinator
    ./build/luigi_coordinator -f "$CONFIG" -b micro -d $DURATION -t $threads -w $owd -x $headroom 2>&1 | \
        tee "${RESULTS_DIR}/1shard_3replicas_${network_name}_t${threads}.txt"

    # Cleanup
    kill $S1_PID $S2_PID $S3_PID 2>/dev/null || true
    pkill -9 luigi_server 2>/dev/null || true

    # Clear network emulation
    tc qdisc del dev lo root 2>/dev/null || true

    sleep 2
    echo ""
}

# Network scenarios: name, delay, jitter, owd, headroom
declare -a scenarios=(
    "same_metro:2:0.5:5:2"
    "same_continent:30:5:40:10"
    "cross_continent:80:10:100:20"
    "cross_region:150:20:180:30"
)

# Thread counts to test
declare -a thread_counts=(1 2 4 8)

# Run all tests
for scenario in "${scenarios[@]}"; do
    IFS=':' read -r name delay jitter owd headroom <<< "$scenario"

    for threads in "${thread_counts[@]}"; do
        run_test "$name" "$delay" "$jitter" "$owd" "$headroom" "$threads"
    done
done

# Final cleanup
tc qdisc del dev lo root 2>/dev/null || true
pkill -9 luigi_server 2>/dev/null || true
pkill -9 luigi_coordinator 2>/dev/null || true

echo "=========================================="
echo "All tests complete!"
echo "Results saved to: $RESULTS_DIR"
echo "=========================================="
