#!/bin/bash
# Run single network test for 1shard-3replicas configuration
set -e

cd /root/cse532/mako

NETWORK_NAME=$1
THREADS=$2
OWD=$3
HEADROOM=$4

if [ -z "$NETWORK_NAME" ] || [ -z "$THREADS" ] || [ -z "$OWD" ] || [ -z "$HEADROOM" ]; then
    echo "Usage: $0 <network_name> <threads> <owd_ms> <headroom_ms>"
    echo ""
    echo "Examples:"
    echo "  $0 same_metro 1 5 2"
    echo "  $0 same_continent 1 40 10"
    echo "  $0 cross_continent 1 100 20"
    echo "  $0 cross_region 1 180 30"
    exit 1
fi

CONFIG="src/deptran/luigi/test/configs/1shard-3replicas.yml"
DURATION=10
RESULTS_DIR="src/deptran/luigi/test/results/network_latency"

mkdir -p "$RESULTS_DIR"

echo "=========================================="
echo "Testing: $NETWORK_NAME - $THREADS threads"
echo "OWD: ${OWD}ms, Headroom: ${HEADROOM}ms"
echo "=========================================="

# Kill any existing processes
pkill -9 luigi_server 2>/dev/null || true
pkill -9 luigi_coordinator 2>/dev/null || true
sleep 1

# Start replicas
echo "Starting replica s101..."
./build/luigi_server -f "$CONFIG" -P s101 > s101_micro.log 2>&1 &
S1_PID=$!
sleep 2

echo "Starting replica s102..."
./build/luigi_server -f "$CONFIG" -P s102 > s102_micro.log 2>&1 &
S2_PID=$!
sleep 2

echo "Starting replica s103..."
./build/luigi_server -f "$CONFIG" -P s103 > s103_micro.log 2>&1 &
S3_PID=$!
sleep 3

# Run coordinator
echo "Running coordinator..."
./build/luigi_coordinator -f "$CONFIG" -b micro -d $DURATION -t $THREADS -w $OWD -x $HEADROOM 2>&1 | \
    tee "${RESULTS_DIR}/1shard_3replicas_${NETWORK_NAME}_t${THREADS}.txt"

# Cleanup
echo ""
echo "Cleaning up..."
kill $S1_PID $S2_PID $S3_PID 2>/dev/null || true
pkill -9 luigi_server 2>/dev/null || true

echo "Done!"
echo "Results saved to: ${RESULTS_DIR}/1shard_3replicas_${NETWORK_NAME}_t${THREADS}.txt"
