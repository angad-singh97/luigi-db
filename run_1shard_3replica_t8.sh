#!/bin/bash
# Focused runner: 1-shard 3-replicas, threads=8 only
# Runs 4 tests (one per network condition)

set -e
cd /root/cse532/mako

# Network conditions: name latency jitter
declare -a NETWORKS=(
  "same_region 2 0.5"
  "same_continent 30 5"
  "cross_continent 80 10"
  "geo_distributed 150 20"
)

# Test parameters
THREADS=8
DURATION=30
CONFIG="1shard_3replicas"
SCRIPT="src/deptran/luigi/test/scripts/run_mako_tpcc_${CONFIG}.sh"
RESULTS_DIR="src/deptran/luigi/test/results/mako_tpcc/${CONFIG}"
CURRENT_TEST=1
TOTAL_TESTS=4

echo "=== Mako TPC-C: 1-Shard 3-Replica Test Suite (Threads=8) ==="
echo "Total tests: $TOTAL_TESTS"
echo "Duration per test: ${DURATION}s"
echo ""

# Function to run a single test
run_test() {
  local network_name=$1
  local latency=$2
  local jitter=$3
  
  local output_dir="${RESULTS_DIR}/t${THREADS}"
  local output_file="${output_dir}/${network_name}.txt"
  
  echo "[$CURRENT_TEST/$TOTAL_TESTS] Running: t${THREADS}/${network_name}"
  
  # Kill any existing processes and clean network
  ps aux | grep -i dbtest | awk "{print \$2}" | xargs kill -9 2>/dev/null || true
  sudo tc qdisc del dev lo root 2>/dev/null || true
  sleep 2
  
  # Run test in background
  # We use a generous timeout (150s) to allow for the very long shutdown sequence
  # observed in high-throughput 1-shard tests
  timeout 150s $SCRIPT $DURATION $THREADS $latency $jitter > "$output_file" 2>&1 &
  TEST_PID=$!
  
  # Wait for the test process
  wait $TEST_PID 2>/dev/null || true
  
  # Force cleanup after test completes (or times out)
  ps aux | grep -i dbtest | awk "{print \$2}" | xargs kill -9 2>/dev/null || true
  sudo tc qdisc del dev lo root 2>/dev/null || true
  sleep 5
  
  # Verify result
  if grep -q "agg_persist_throughput" "$output_file"; then
    local throughput=$(grep "agg_persist_throughput" "$output_file" | head -1 | awk '{print $2}')
    echo "  ✓ Complete - ${throughput} ops/sec"
  else
    echo "  ✗ FAILED - no results found"
    # Print last few lines to see what happened
    tail -n 5 "$output_file"
  fi
  
  CURRENT_TEST=$((CURRENT_TEST + 1))
}

# Run tests
for network_spec in "${NETWORKS[@]}"; do
  read -r network_name latency jitter <<< "$network_spec"
  run_test "$network_name" "$latency" "$jitter"
done

echo ""
echo "=== Test Suite Complete ==="
