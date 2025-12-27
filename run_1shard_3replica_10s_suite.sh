#!/bin/bash
# Dedicated runner for 1-shard 3-replicas (t1, t2, t4) with 10s duration
# This uses the new --duration flag support to avoid shutdown hangs

set -e
cd /root/cse532/mako

# Network conditions: name latency jitter
declare -a NETWORKS=(
  "same_region 2 0.5"
  "same_continent 30 5"
  "cross_continent 80 10"
  "geo_distributed 150 20"
)

# Thread counts to run
declare -a THREAD_COUNTS=(4 2 1)
DURATION=10
CONFIG="1shard_3replicas"
SCRIPT="src/deptran/luigi/test/scripts/run_mako_tpcc_${CONFIG}.sh"
RESULTS_DIR="src/deptran/luigi/test/results/mako_tpcc/${CONFIG}"

echo "=== Mako TPC-C: 1-Shard 3-Replica 10s Test Suite ==="
echo "Threads: ${THREAD_COUNTS[*]}"
echo "Duration: ${DURATION}s"
echo ""

# Function to run a single test
run_test() {
  local threads=$1
  local network_name=$2
  local latency=$3
  local jitter=$4
  
  local output_dir="${RESULTS_DIR}/t${threads}"
  mkdir -p "$output_dir"
  local output_file="${output_dir}/${network_name}.txt"
  
  echo "Running: t${threads}/${network_name} (10s)..."
  
  # Kill any existing processes and clean network
  ps aux | grep -i dbtest | awk "{print \$2}" | xargs kill -9 2>/dev/null || true
  sudo tc qdisc del dev lo root 2>/dev/null || true
  sleep 2
  
  # Run test in background with generous timeout for shutdown
  # We use the script which now properly accepts DURATION as arg 1 and passes it to shard.sh
  nohup $SCRIPT $DURATION $threads $latency $jitter > "$output_file" 2>&1 &
  TEST_PID=$!
  
  # Wait for the test process
  wait $TEST_PID
  
  # Force cleanup after test completes to be sure
  ps aux | grep -i dbtest | awk "{print \$2}" | xargs kill -9 2>/dev/null || true
  sudo tc qdisc del dev lo root 2>/dev/null || true
  sleep 2
  
  # Verify result
  if grep -q "agg_persist_throughput" "$output_file"; then
    local throughput=$(grep "agg_persist_throughput" "$output_file" | head -1 | awk '{print $2}')
    echo "  ✓ Complete - ${throughput} ops/sec"
  else
    echo "  ✗ FAILED - no results found"
  fi
}

# Run tests
for threads in "${THREAD_COUNTS[@]}"; do
  for network_spec in "${NETWORKS[@]}"; do
    read -r network_name latency jitter <<< "$network_spec"
    run_test "$threads" "$network_name" "$latency" "$jitter"
  done
done

echo ""
echo "=== Test Suite Complete ==="
