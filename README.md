# Mako + Jetpack (Merged Codebase)

![CI](https://github.com/makodb/mako/actions/workflows/ci.yml/badge.svg)

This repository contains the merged codebase of **Mako (Paxos-based distributed transactions)** and **Jetpack (Raft-based consensus with failure recovery)**. The unified build system supports both protocols through CMake.

## Table of Contents
- [Quickstart](#quickstart)
- [Build System](#build-system)
- [Running Tests](#running-tests)
- [Running Paxos (Mako)](#running-paxos-mako)
- [Running Raft (Jetpack)](#running-raft-jetpack)
- [Configuration](#configuration)
- [Troubleshooting](#troubleshooting)

## Quickstart

### Prerequisites

This is tested on Debian 12 and Ubuntu 22.04.

### Clone the Repository

```bash
git clone --recursive https://github.com/makodb/mako.git
cd mako
```

### Install Dependencies

```bash
bash apt_packages.sh
source install_rustc.sh
```

### Build Everything

```bash
# Clean build (recommended after git pull or configuration changes)
rm -rf build && mkdir build
cmake -B build -DRAFT_TEST=OFF -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build -j32

# Or use the convenience script
bash compile-cmake.sh
```

**Important**: The `-DREUSE_CORO` flag is now enabled by default in CMakeLists.txt. This is **required for Raft stability**.

## Build System

### Build Targets

The unified CMake build system provides the following executables:

- **`dbtest`**: Main executable for Paxos/Mako experiments and benchmarks
- **`deptran_server`**: Server executable for Raft/Jetpack consensus

### Build Commands

```bash
# Clean build (recommended after git pull or configuration changes)
rm -rf build && mkdir build
cmake -B build -DRAFT_TEST=OFF -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build -j32

# Or use the convenience script
bash compile-cmake.sh

# If you run on your PC, you can use fewer CPU cores (e.g., -j4)
make -j4
```

**Important**: The `-DREUSE_CORO` flag is now enabled by default in CMakeLists.txt. This is **required for Raft stability**.

You should now see libmako.a and a few examples in the build folder, and run all examples via `./ci/ci.sh all`


## erpc - socket implementation test
```bash
cd ./third-party/erpc
rm -rf CMakeFiles cmake_install.cmake CMakeCache.txt
cmake . -DTRANSPORT=fake -DROCE=off -DPERF=off
make 
make latency
```

* Edit the file `scripts/autorun_process_file` like below (server first, then client) 
```
130.245.173.102 31850 0
130.245.173.103 32850 0
```
  * We use port `31850` and `32850` to establish the connection for first time, then we use `31850+10000` and `32850+10000` to exchange messages 

* Run the eRPC application (the latency benchmark by default):
  * At 130.245.173.102: `./scripts/do.sh 0 0 eth`
  * At 130.245.173.103: `./scripts/do.sh 1 0 eth`


## Several assumptions
1. All servers, including all followers, and learners, are managed via nfs. We use it to do some execution flow control.


Run the helloworld:

```bash
# Configure with CMake
cmake -B build [OPTIONS]

# Build specific targets
cmake --build build --target dbtest -j32
cmake --build build --target deptran_server -j32

# Or build all targets
cmake --build build -j32
```

### CMake Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `RAFT_TEST` | `OFF` | Enable Raft lab testing mode (uses coroutine-based execution) |
| `PAXOS_LIB_ENABLED` | `1` | Enable Paxos library |
| `SHARDS` | `3` | Number of shards for distributed transactions |
| `MICRO_BENCHMARK` | `0` | Enable micro-benchmarking |
| `CMAKE_EXPORT_COMPILE_COMMANDS` | `OFF` | Generate compile_commands.json for IDEs |

#### Examples:

```bash
# Production Raft build (default)
cmake -B build -DRAFT_TEST=OFF

# Raft lab testing build
cmake -B build -DRAFT_TEST=ON

# Custom shard configuration
cmake -B build -DSHARDS=5 -DPAXOS_LIB_ENABLED=1
```

### Build Time Expectations

**WARNING**: This is a large C++ project with extensive dependencies. Build times:
- **Initial full build**: 10-30 minutes (depends on CPU cores and parallelism)
- **Incremental builds**: 2-10 minutes
- **Clean rebuild**: 15-30 minutes

**Always use longer timeouts for builds** (10+ minutes minimum, or no timeout).

### Important Build Notes

#### REUSE_CORO Flag (Critical for Raft)

The `-DREUSE_CORO` compiler flag is **required for Raft stability**. It's now enabled by default in CMakeLists.txt (line 281). Without this flag, Raft will crash with term mismatch errors.

**To verify it's enabled:**
```bash
grep -o "\-DREUSE_CORO" build/compile_commands.json | wc -l
# Should show ~250+ occurrences
```

#### After Updating CMakeLists.txt

**Always reconfigure CMake after modifying CMakeLists.txt:**
```bash
# Wrong (will use old configuration)
make

# Correct (regenerates build files)
rm -rf build && mkdir build
cmake -B build -DRAFT_TEST=OFF
cmake --build build -j32
```

## Running Tests

### Continuous Integration Tests

```bash
# Run all tests
./ci/ci.sh all

# Run specific test
./ci/ci.sh simplePaxos
./ci/ci.sh shard1Replication
./ci/ci.sh shard2Replication
```

### Python Test Runner

```bash
# Run with Janus protocol
python3 test_run.py -m janus

# Run all experiments
python3 run_all.py
```

## Running Paxos (Mako)

### Single Shard with Paxos Replication (1 Leader + 2 Followers + 1 Learner)

To test Paxos replication with multiple replicas on a single machine, run these commands in **separate terminals**:

```bash
# Terminal 1: Follower 1 (p1)
nohup ./build/dbtest --verbose --bench tpcc --basedir ./tmp \
                     --db-type mbta --num-threads 6 --scale-factor 6 --num-erpc-server 2 \
                     --shard-index 0 --shard-config $(pwd)/src/mako/config/local-shards1-warehouses6.yml \
                     -F config/1leader_2followers/paxos6_shardidx0.yml -F config/occ_paxos.yml \
                     --txn-flags 1 --runtime 30 -P p1 --bench-opts \
                     --new-order-fast-id-gen --retry-aborted-transactions --numa-memory 1G > p1.log 2>&1 &

# Terminal 2: Follower 2 (p2)
nohup ./build/dbtest --verbose --bench tpcc --basedir ./tmp \
                     --db-type mbta --num-threads 6 --scale-factor 6 --num-erpc-server 2 \
                     --shard-index 0 --shard-config $(pwd)/src/mako/config/local-shards1-warehouses6.yml \
                     -F config/1leader_2followers/paxos6_shardidx0.yml -F config/occ_paxos.yml \
                     --txn-flags 1 --runtime 30 -P p2 --bench-opts \
                     --new-order-fast-id-gen --retry-aborted-transactions --numa-memory 1G > p2.log 2>&1 &

# Terminal 3: Leader (localhost)
nohup ./build/dbtest --verbose --bench tpcc --basedir ./tmp \
                     --db-type mbta --num-threads 6 --scale-factor 6 --num-erpc-server 2 \
                     --shard-index 0 --shard-config $(pwd)/src/mako/config/local-shards1-warehouses6.yml \
                     -F config/1leader_2followers/paxos6_shardidx0.yml -F config/occ_paxos.yml \
                     --txn-flags 1 --runtime 30 -P localhost --bench-opts \
                     --new-order-fast-id-gen --retry-aborted-transactions --numa-memory 1G > leader.log 2>&1 &

# Terminal 4: Learner (learner)
nohup ./build/dbtest --verbose --bench tpcc --basedir ./tmp \
                     --db-type mbta --num-threads 6 --scale-factor 6 --num-erpc-server 2 \
                     --shard-index 0 --shard-config $(pwd)/src/mako/config/local-shards1-warehouses6.yml \
                     -F config/1leader_2followers/paxos6_shardidx0.yml -F config/occ_paxos.yml \
                     --txn-flags 1 --runtime 30 -P learner --bench-opts \
                     --new-order-fast-id-gen --retry-aborted-transactions --numa-memory 1G > learner.log 2>&1 &
```

**Monitor logs:**
```bash
tail -f leader.log p1.log p2.log learner.log
```

**Stop all processes:**
```bash
pkill -f dbtest
```

### Multi-site Testing (Single Process with All Sites)

```bash
export MAKO_MULTI_SITE=1 && ./build/dbtest -t 6 -s 1 -S 1 -K 3 \
  -N leader_s0,follower1_s0,follower2_s0 \
  -C ./multi_site_config.yml \
  -F ./paxos_multi_site.yml \
  -F ../config/occ_paxos.yml
```

## Running Raft (Jetpack)

Raft uses the `deptran_server` executable. After building with `make -j32`, you can run Raft with different configurations.

### Production Raft Configurations

**Basic Raft (local 3 machines, closed-loop 1×1 clients):**
```bash
./build/deptran_server \
  -f config/none_raft.yml \
  -f config/1c1s3r1p.yml \
  -f config/rw.yml \
  -f config/client_closed.yml \
  -f config/concurrent_1.yml \
  -d 30 -m 100 -P localhost
```

**Raft with higher concurrency (12×12 clients, high throughput ~25k TPS):**
```bash
./build/deptran_server \
  -f config/none_raft.yml \
  -f config/12c1s3r1p.yml \
  -f config/rw.yml \
  -f config/client_closed.yml \
  -f config/concurrent_12.yml \
  -d 30 -m 100 -P localhost
```

**Raft + Jetpack failure recovery:**
```bash
./build/deptran_server \
  -f config/rule_raft.yml \
  -f config/1c1s3r1p.yml \
  -f config/rw.yml \
  -f config/client_closed.yml \
  -f config/concurrent_1.yml \
  -f config/failover.yml \
  -d 30 -m 100 -P localhost
```

### Raft Lab Tests

For testing with coroutine-based execution:

```bash
# Build with RAFT_TEST enabled
cmake -B build -DRAFT_TEST=ON
cmake --build build -j32

# Run Raft lab tests
./build/deptran_server -f config/raft_lab_test.yml
```

## Configuration

### Host Configuration

Update host configuration for distributed testing:
```bash
bash ./src/mako/update_config.sh
```

Configuration files are in `config/`:
- **Host topology**: `config/hosts*.yml`
- **Benchmark settings**: `config/*.yml` (e.g., `config/rw.yml`, `config/client_closed.yml`)
- **Protocol-specific**: `config/occ_paxos.yml`, `config/none_raft.yml`, `config/rule_raft.yml`

### Paxos Configuration Files

- `config/1leader_2followers/paxos6_shardidx0.yml`: 1 leader + 2 followers + 1 learner setup
- `src/mako/config/local-shards1-warehouses6.yml`: Shard and warehouse configuration

### Raft Configuration Files

- `config/none_raft.yml`: Basic Raft without Jetpack recovery
- `config/rule_raft.yml`: Raft with Jetpack failure recovery enabled
- `config/raft_lab_test.yml`: Lab testing configuration
- `config/1c1s3r1p.yml`: 1 client, 1 shard, 3 replicas, 1 partition
- `config/concurrent_*.yml`: Concurrency level (1, 12, etc.)


## Results Processing

```bash
python3 results_processor.py <Experiment time (directory name under results folder)>

# Example:
python3 results_processor.py 2023-10-10-03:38:03
```

## Project Structure

```
mako/
├── src/
│   ├── deptran/          # Transaction protocols (Janus, 2PL, OCC, Paxos, Raft)
│   │   ├── paxos/        # Paxos/Mako implementation
│   │   ├── raft/         # Raft/Jetpack implementation
│   │   ├── janus/        # Janus protocol
│   │   └── ...
│   ├── mako/             # Mako-specific code (Masstree, benchmarks)
│   ├── bench/            # Benchmark implementations (TPC-C, TPC-A, RW)
│   ├── rrr/              # Custom RPC framework
│   └── memdb/            # In-memory database backend
├── config/               # YAML configuration files
├── build/                # Build output directory
├── CMakeLists.txt        # Unified CMake build configuration
└── README.md             # This file
```

