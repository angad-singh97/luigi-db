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

This is tested on Debian 12 and Ubuntu 22.04+.

### Clone the Repository

```bash
git clone --recursive https://github.com/makodb/mako.git
cd mako
```

### Install Dependencies

```bash
bash apt_packages.sh
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

### Building for Raft

Raft uses the `deptran_server` executable:

```bash
# Production Raft (default)
cmake -B build -DRAFT_TEST=OFF
cmake --build build --target deptran_server -j32

# Raft lab testing (with coroutine-based execution)
cmake -B build -DRAFT_TEST=ON
cmake --build build --target deptran_server -j32
```

### Running Raft with Different Configurations

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

**Raft with higher concurrency (12×12 clients):**
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

```bash
# Build with RAFT_TEST enabled
cmake -B build -DRAFT_TEST=ON
cmake --build build --target deptran_server -j32

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

## Troubleshooting

### Raft Crashes with "verify failed: currentTerm == term"

**Cause**: Missing `-DREUSE_CORO` compiler flag.

**Solution**: The flag is now enabled by default. If you're experiencing this:
1. Clean rebuild:
   ```bash
   rm -rf build && mkdir build
   cmake -B build -DRAFT_TEST=OFF
   cmake --build build -j32
   ```
2. Verify the flag is present:
   ```bash
   grep "\-DREUSE_CORO" build/compile_commands.json
   ```

### Build Fails After Modifying CMakeLists.txt

**Cause**: Stale CMake cache.

**Solution**: Always reconfigure CMake:
```bash
rm -rf build && mkdir build
cmake -B build [OPTIONS]
cmake --build build -j32
```

### "RAFT_TEST_CORO enabled" Error in Production

**Cause**: Wrong CMake configuration.

**Solution**: Reconfigure with `-DRAFT_TEST=OFF`:
```bash
cmake -B build -DRAFT_TEST=OFF
cmake --build build --target deptran_server -j32
```

### Build Timeout

**Cause**: Short timeout on large codebase.

**Solution**: Use longer timeout (10+ minutes) or no timeout:
```bash
# Don't use short timeouts like 60s or 120s
timeout 1800 cmake --build build -j32  # 30 minutes
# Or just let it finish
cmake --build build -j32
```

### Performance Regression After Merge

**Known Issue**: The merged codebase has a 3x performance regression in Paxos replication throughput due to `shared_ptr` overhead in RPC hot paths. This causes `shard1Replication` test to fail (requires replay_batch > 1000, gets ~800).

**Workaround**: Tests with lower thresholds (e.g., `shard1ReplicationSimple`) pass successfully.

## Results Processing

```bash
python3 results_processor.py <Experiment time (directory name under results folder)>

# Example:
python3 results_processor.py 2023-10-10-03:38:03
```

## Additional Resources

- **Architecture documentation**: `COMPATIBILITY_ANALYSIS.md`
- **Merge documentation**: `MERGE_DOCUMENTATION.md`
- **Original Mako README**: `OLD-README.md`
- **TODO list**: `doc/TODO.md`
- **EC2 deployment**: `doc/ec2.md`
- **Profiling guide**: `doc/profile.md`

## Project Structure

```
mako_temp/
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

## License

See the LICENSE file in the repository.
