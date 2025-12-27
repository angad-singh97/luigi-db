#!/bin/bash
# Re-run failed 1-shard 3-replica tests with longer timeout

set -e
cd /root/cse532/mako

DURATION=30
CONFIG="1shard_3replicas"
SCRIPT="src/deptran/luigi/test/scripts/run_mako_tpcc_${CONFIG}.sh"
RESULTS_DIR="src/deptran/luigi/test/results/mako_tpcc/${CONFIG}"

echo "=== Re-running Failed 1-Shard 3-Replica Tests ==="
echo "Tests to re-run: 9"
echo "Timeout: 100 seconds per test"
echo ""

# Function to run a single test
run_test() {
  local threads=$1
  local network_name=$2
  local latency=$3
  local jitter=$4
  local test_num=$5
  
  local output_dir="${RESULTS_DIR}/t${threads}"
  local output_file="${output_dir}/${network_name}.txt"
  
  echo "[$test_num/9] Running: t${threads}/${network_name}"
  
  # Kill any existing processes and clean network
  ps aux | grep -i dbtest | awk "{print \$2}" | xargs kill -9 2>/dev/null || true
  sudo tc qdisc del dev lo root 2>/dev/null || true
  sleep 2
  
  # Run test in background with 100s timeout
  timeout 100s $SCRIPT $DURATION $threads $latency $jitter > "$output_file" 2>&1 || true
  
  # Cleanup
  ps aux | grep -i dbtest | awk "{print \$2}" | xargs kill -9 2>/dev/null || true
  sudo tc qdisc del dev lo root 2>/dev/null || true
  sleep 5
  
  # Verify result
  if grep -q "agg_persist_throughput" "$output_file"; then
    local throughput=$(grep "agg_persist_throughput" "$output_file" | head -1 | awk '{print $2}')
    echo "  ✓ Complete - ${throughput} ops/sec"
  else
    echo "  ✗ FAILED - no results found"
  fi
}

# Re-run failed tests
run_test 2 "same_region" 2 0.5 1
run_test 4 "same_region" 2 0.5 2
run_test 4 "same_continent" 30 5 3
run_test 4 "cross_continent" 80 10 4
run_test 4 "geo_distributed" 150 20 5
run_test 8 "same_region" 2 0.5 6
run_test 8 "same_continent" 30 5 7
run_test 8 "cross_continent" 80 10 8
run_test 8 "geo_distributed" 150 20 9

echo ""
echo "=== Re-run Complete ==="
