#!/bin/bash
# Test runner for 1-shard 3-replica configuration only
# Runs 16 tests: 4 thread counts × 4 network conditions

set -e
cd /root/cse532/mako

# Network conditions: name latency jitter
declare -a NETWORKS=(
  "same_region 2 0.5"
  "same_continent 30 5"
  "cross_continent 80 10"
  "geo_distributed 150 20"
)

# Thread counts
declare -a THREADS=(1 2 4 8)

DURATION=30
CONFIG="1shard_3replicas"
SCRIPT="src/deptran/luigi/test/scripts/run_mako_tpcc_${CONFIG}.sh"
RESULTS_DIR="src/deptran/luigi/test/results/mako_tpcc/${CONFIG}"
TOTAL_TESTS=16
CURRENT_TEST=1

echo "=== Mako TPC-C: 1-Shard 3-Replica Test Suite ==="
echo "Total tests: $TOTAL_TESTS"
echo "Duration per test: ${DURATION}s"
echo "Estimated time: ~19 minutes"
echo ""

# Function to run a single test
run_test() {
  local threads=$1
  local network_name=$2
  local latency=$3
  local jitter=$4
  
  local output_dir="${RESULTS_DIR}/t${threads}"
  local output_file="${output_dir}/${network_name}.txt"
  
  echo "[$CURRENT_TEST/$TOTAL_TESTS] Running: t${threads}/${network_name}"
  
  # Kill any existing processes and clean network
  ps aux | grep -i dbtest | awk "{print \$2}" | xargs kill -9 2>/dev/null || true
  sudo tc qdisc del dev lo root 2>/dev/null || true
  sleep 2
  
  # Run test in background and save all output
  $SCRIPT $DURATION $threads $latency $jitter > "$output_file" 2>&1 &
  TEST_PID=$!
  
  # Wait for benchmark duration + 40 second buffer for startup/shutdown
  sleep $((DURATION + 40))
  
  # Aggressively kill all dbtest processes (don't wait for graceful shutdown)
  ps aux | grep -i dbtest | awk "{print \$2}" | xargs kill -9 2>/dev/null || true
  wait $TEST_PID 2>/dev/null || true
  
  # Cleanup and wait
  ps aux | grep -i dbtest | awk "{print \$2}" | xargs kill -9 2>/dev/null || true
  sudo tc qdisc del dev lo root 2>/dev/null || true
  sleep 10
  
  # Verify result
  if grep -q "agg_persist_throughput" "$output_file"; then
    local throughput=$(grep "agg_persist_throughput" "$output_file" | head -1 | awk '{print $2}')
    echo "  ✓ Complete - ${throughput} ops/sec"
  else
    echo "  ✗ FAILED - no results found"
  fi
  
  CURRENT_TEST=$((CURRENT_TEST + 1))
}

# Run all tests
for threads in "${THREADS[@]}"; do
  for network_spec in "${NETWORKS[@]}"; do
    read -r network_name latency jitter <<< "$network_spec"
    run_test "$threads" "$network_name" "$latency" "$jitter"
  done
done

echo ""
echo "=== Test Suite Complete ==="
echo "Results saved in: $RESULTS_DIR"
echo "Total tests completed: $TOTAL_TESTS"
