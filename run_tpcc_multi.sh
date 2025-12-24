#!/bin/bash
# Run TPCC multi-shard benchmark (2 shards)

set -e

cd /root/cse532/mako
export LD_LIBRARY_PATH=/root/cse532/mako/build:$LD_LIBRARY_PATH

# Kill any existing processes
pkill -9 luigi_server 2>/dev/null || true
pkill -9 luigi_coordinator 2>/dev/null || true
sleep 1

CONFIG="src/deptran/luigi/config/local-2shard-proc.yml"
DURATION="${1:-10}"

echo "=== Starting TPCC Multi-Shard Benchmark ==="
echo "Shards: 2"
echo "Duration: ${DURATION}s"
echo ""

# Start shard0 server
echo "Starting shard0 server..."
./build/luigi_server -f "$CONFIG" -P shard0 > shard0_tpcc_multi.log 2>&1 &
SHARD0_PID=$!
sleep 2

# Start shard1 server
echo "Starting shard1 server..."
./build/luigi_server -f "$CONFIG" -P shard1 > shard1_tpcc_multi.log 2>&1 &
SHARD1_PID=$!
sleep 3

# Run coordinator with TPCC benchmark
echo "Running TPCC multi-shard benchmark..."
./build/luigi_coordinator -f "$CONFIG" -b tpcc -d "$DURATION" 2>&1 | tee coord_tpcc_multi.log

# Cleanup
echo ""
echo "Cleaning up..."
kill $SHARD0_PID $SHARD1_PID 2>/dev/null || true
pkill -9 luigi_server 2>/dev/null || true

echo "Done!"
