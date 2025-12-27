#!/bin/bash
# Targeted runner for T4 tests only

set -e
cd /root/cse532/mako

DURATION=10
CONFIG="1shard_3replicas"
SCRIPT="src/deptran/luigi/test/scripts/run_mako_tpcc_${CONFIG}.sh"
RESULTS_DIR="src/deptran/luigi/test/results/mako_tpcc/${CONFIG}"

echo "=== Mako TPC-C: Targeted T4 Tests (10s) ==="

run_target() {
    local threads=$1
    local name=$2
    local lat=$3
    local jit=$4
    
    echo "Running: t${threads}/${name}..."
    ps aux | grep -i dbtest | awk "{print \$2}" | xargs kill -9 2>/dev/null || true
    sudo tc qdisc del dev lo root 2>/dev/null || true
    sleep 2
    
    nohup $SCRIPT $DURATION $threads $lat $jit > "${RESULTS_DIR}/t${threads}/${name}.txt" 2>&1 &
    PID=$!
    wait $PID
    
    ps aux | grep -i dbtest | awk "{print \$2}" | xargs kill -9 2>/dev/null || true
    sudo tc qdisc del dev lo root 2>/dev/null || true
    sleep 2
}

# 3. t4 Suite
run_target 4 "same_region" 2 0.5
run_target 4 "same_continent" 30 5
run_target 4 "cross_continent" 80 10
run_target 4 "geo_distributed" 150 20

echo "Done running t4 tests."
