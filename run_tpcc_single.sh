#!/bin/bash
# Run TPCC single shard benchmark

set -e

cd /root/cse532/mako
export LD_LIBRARY_PATH=/root/cse532/mako/build:$LD_LIBRARY_PATH

# Kill any existing processes
pkill -9 luigi_server 2>/dev/null || true
pkill -9 luigi_coordinator 2>/dev/null || true
sleep 1

CONFIG="src/deptran/luigi/config/local-1shard-proc.yml"
DURATION="${1:-10}"

echo "=== Starting TPCC Single Shard Benchmark ==="
echo "Duration: ${DURATION}s"
echo ""

# Start server
echo "Starting shard0 server..."
./build/luigi_server -f "$CONFIG" -P shard0 > shard0_tpcc.log 2>&1 &
SERVER_PID=$!
sleep 3

# Run coordinator with TPCC benchmark
echo "Running TPCC benchmark..."
./build/luigi_coordinator -f "$CONFIG" -b tpcc -d "$DURATION" 2>&1 | tee coord_tpcc.log

# Cleanup
echo ""
echo "Cleaning up..."
kill $SERVER_PID 2>/dev/null || true
pkill -9 luigi_server 2>/dev/null || true

echo "Done!"
