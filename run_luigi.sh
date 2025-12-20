#!/bin/bash
# run_luigi.sh - Luigi benchmark launcher script
#
# Usage:
#   ./run_luigi.sh <config.yml> [options]
#
# Example:
#   ./run_luigi.sh src/deptran/luigi/config/local-2shard.yml -d 30 -b micro

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
CONFIG="$1"
shift || true

# Check arguments
if [ -z "$CONFIG" ]; then
    echo "Usage: $0 <config.yml> [coordinator options]"
    echo ""
    echo "Example:"
    echo "  $0 src/deptran/luigi/config/local-2shard.yml -d 30 -b micro"
    exit 1
fi

if [ ! -f "$CONFIG" ]; then
    echo "Error: Config file not found: $CONFIG"
    exit 1
fi

if [ ! -f "$BUILD_DIR/luigi_server" ] || [ ! -f "$BUILD_DIR/luigi_coordinator" ]; then
    echo "Error: Build executables not found."
    echo "Run: cmake --build build --target luigi_coordinator luigi_server"
    exit 1
fi

# Extract server process names from config (format: "  s0: localhost")
PROCS=$(grep -E "^  s[0-9]+:" "$CONFIG" | sed 's/:.*//' | tr -d ' ')

echo "=== Luigi Benchmark ==="
echo "Config: $CONFIG"
echo "Servers: $PROCS"
echo ""

# Start servers
PIDS=()
for PROC in $PROCS; do
    echo "Starting server: $PROC"
    "$BUILD_DIR/luigi_server" -f "$CONFIG" -P "$PROC" &
    PIDS+=($!)
done

# Wait for servers to initialize
echo "Waiting for servers to start..."
sleep 3

echo ""
echo "Starting coordinator..."
echo ""

# Run coordinator
"$BUILD_DIR/luigi_coordinator" -f "$CONFIG" "$@"

# Cleanup
echo ""
echo "Stopping servers..."
for PID in "${PIDS[@]}"; do
    kill "$PID" 2>/dev/null || true
done
wait 2>/dev/null

echo "Done!"
