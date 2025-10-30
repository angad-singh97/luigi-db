# Mako + Jetpack

<div align="center">

![CI](https://github.com/makodb/mako/actions/workflows/ci.yml/badge.svg)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![OSDI'25](https://img.shields.io/badge/OSDI'25-Mako-orange.svg)](#what-is-mako)

**High-Performance Distributed Transactions with Paxos & Raft Replication**

[What is Mako?](#what-is-mako) • [Why Choose Mako](#why-choose-mako) • [Quick Start](#quick-start) • [Build System](#build-system) • [Run Paxos](#running-paxos-mako) • [Run Raft](#running-raft-jetpack)

</div>

---

## What is Mako?

**Mako** is a high-performance distributed transactional key-value store with geo-replication support, built on state-of-the-art systems research. Its core innovation decouples transaction execution from replication through watermark-based speculative execution. Leaders execute transactions immediately, while replication happens asynchronously; followers validate via watermarks before exposing results, ensuring serializable consistency. The system powers millions of TPC-C transactions per second across continents and is detailed in our [OSDI'25 paper](https://www.usenix.org/conference/osdi25/presentation/shen-weihai).

The repository also bundles **Jetpack**, a Raft-based consensus layer with advanced failover recovery. The unified build lets you choose Paxos (legacy Mako) or Raft/Jetpack at compile time.

---

## Why Choose Mako?

### Proven Research & Performance
- Backed by peer-reviewed OSDI'25 results with **3.66M TPC-C txn/s** under geo-replication
- Demonstrates **8.6× higher throughput** than prior state-of-the-art systems

### Core Capabilities
- **Serializable distributed transactions** with strong ACID guarantees
- **Geo-replication** with automatic sharding and configurable consistency
- **High-performance storage** via Masstree (in-memory) and RocksDB persistence
- **Horizontal scalability** across partitions and replicas
- **Fault tolerance** through Paxos or Raft consensus plus Jetpack recovery
- **RustyCpp-powered safety** for lifetime tracking in critical components

### Developer Friendly
- Industry benchmarks: TPC-C, TPC-A, read/write, microbenchmarks
- RocksDB-like interfaces and Redis-compatible layer
- Comprehensive CMake + Make build support and CI harness
- Modular design for experimenting with consensus and concurrency protocols

---

## Quick Start

Tested on **Debian 12** and **Ubuntu 22.04**.

```bash
# 1. Clone (with submodules)
git clone --recursive https://github.com/makodb/mako.git
cd mako

# 2. Install dependencies
bash apt_packages.sh
source install_rustc.sh

# 3. Configure & build (Raft tests off by default)
rm -rf build && mkdir build
cmake -B build -DRAFT_TEST=OFF -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build -j32   # use -j4 on laptops

# Convenience script
bash compile-cmake.sh
```

> **Important:** The `-DREUSE_CORO` flag is enabled by default (see `CMakeLists.txt`). It is **required** for Raft/Jetpack stability—verify with `grep -o "\-DREUSE_CORO" build/compile_commands.json`.

After a successful build you will find the primary binaries (`dbtest`, `deptran_server`, etc.) under `build/`.

---

## Build System

### Targets
- `dbtest` – Paxos/Mako experiments, benchmarks, and most CI tests
- `deptran_server` – Raft/Jetpack server executable (plus Raft lab harness)
- Libraries (`libmako.a`, etc.) consumed by examples and tests

### Typical Workflows

```bash
# Reconfigure after touching CMake files
rm -rf build && mkdir build
cmake -B build -DRAFT_TEST=OFF
cmake --build build -j32

# Build individual targets
cmake --build build --target dbtest -j32
cmake --build build --target deptran_server -j32

# Makefile fallback
make -j32       # or -j4 on smaller machines
```

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `RAFT_TEST` | `OFF` | Enable Raft coroutine-based lab suite |
| `PAXOS_LIB_ENABLED` | `1` | Build Paxos components |
| `SHARDS` | `3` | Default shard count for benchmark configs |
| `MICRO_BENCHMARK` | `0` | Enable microbench harness |
| `CMAKE_EXPORT_COMPILE_COMMANDS` | `OFF` | Generate `compile_commands.json` |

Examples:

```bash
# Production Raft build (no lab harness)
cmake -B build -DRAFT_TEST=OFF

# Raft lab-testing build
cmake -B build -DRAFT_TEST=ON

# Custom shard topology
cmake -B build -DSHARDS=5 -DPAXOS_LIB_ENABLED=1
```

### eRPC Socket Smoke Test

```bash
cd third-party/erpc
rm -rf CMakeFiles cmake_install.cmake CMakeCache.txt
cmake . -DTRANSPORT=fake -DROCE=off -DPERF=off
make && make latency
```

Edit `scripts/autorun_process_file` with your server/client hosts, then run:

```bash
./scripts/do.sh 0 0 eth    # server host
./scripts/do.sh 1 0 eth    # client host
```

---

## Running Tests

```bash
# Run entire CI suite
./ci/ci.sh all

# Focused tests
./ci/ci.sh simpleTransaction
./ci/ci.sh simplePaxos
./ci/ci.sh shard1Replication
./ci/ci.sh shard2Replication

# Python harness
python3 test_run.py -m janus
python3 run_all.py
```

Build times are substantial: expect 10–30 minutes for a clean build, 2–10 minutes incrementally. Adjust CI or local timeouts accordingly.

---

## Running Paxos (Mako)

### 1 Leader + 2 Followers + 1 Learner (single machine, multi-process)

Run each command in its own terminal:

```bash
# Follower p1
nohup ./build/dbtest --verbose --bench tpcc --basedir ./tmp \
  --db-type mbta --num-threads 6 --scale-factor 6 --num-erpc-server 2 \
  --shard-index 0 --shard-config $(pwd)/src/mako/config/local-shards1-warehouses6.yml \
  -F config/1leader_2followers/paxos6_shardidx0.yml -F config/occ_paxos.yml \
  --txn-flags 1 --runtime 30 -P p1 --bench-opts \
  --new-order-fast-id-gen --retry-aborted-transactions --numa-memory 1G > p1.log 2>&1 &

# Follower p2
nohup ./build/dbtest --verbose --bench tpcc --basedir ./tmp \
  --db-type mbta --num-threads 6 --scale-factor 6 --num-erpc-server 2 \
  --shard-index 0 --shard-config $(pwd)/src/mako/config/local-shards1-warehouses6.yml \
  -F config/1leader_2followers/paxos6_shardidx0.yml -F config/occ_paxos.yml \
  --txn-flags 1 --runtime 30 -P p2 --bench-opts \
  --new-order-fast-id-gen --retry-aborted-transactions --numa-memory 1G > p2.log 2>&1 &

# Leader localhost
nohup ./build/dbtest --verbose --bench tpcc --basedir ./tmp \
  --db-type mbta --num-threads 6 --scale-factor 6 --num-erpc-server 2 \
  --shard-index 0 --shard-config $(pwd)/src/mako/config/local-shards1-warehouses6.yml \
  -F config/1leader_2followers/paxos6_shardidx0.yml -F config/occ_paxos.yml \
  --txn-flags 1 --runtime 30 -P localhost --bench-opts \
  --new-order-fast-id-gen --retry-aborted-transactions --numa-memory 1G > leader.log 2>&1 &

# Learner
nohup ./build/dbtest --verbose --bench tpcc --basedir ./tmp \
  --db-type mbta --num-threads 6 --scale-factor 6 --num-erpc-server 2 \
  --shard-index 0 --shard-config $(pwd)/src/mako/config/local-shards1-warehouses6.yml \
  -F config/1leader_2followers/paxos6_shardidx0.yml -F config/occ_paxos.yml \
  --txn-flags 1 --runtime 30 -P learner --bench-opts \
  --new-order-fast-id-gen --retry-aborted-transactions --numa-memory 1G > learner.log 2>&1 &
```

Monitor and clean up:

```bash
tail -f leader.log p1.log p2.log learner.log
pkill -f dbtest
```

### Multi-site (single process)

```bash
export MAKO_MULTI_SITE=1
./build/dbtest -t 6 -s 1 -S 1 -K 3 \
  -N leader_s0,follower1_s0,follower2_s0 \
  -C ./multi_site_config.yml \
  -F ./paxos_multi_site.yml \
  -F ../config/occ_paxos.yml
```

---

## Running Raft (Jetpack)

Build `deptran_server`, then choose a configuration:

```bash
# Basic Raft (1 client, 1 shard, 3 replicas)
./build/deptran_server \
  -f config/none_raft.yml \
  -f config/1c1s3r1p.yml \
  -f config/rw.yml \
  -f config/client_closed.yml \
  -f config/concurrent_1.yml \
  -d 30 -m 100 -P localhost

# Higher concurrency (12x12 clients, ~25k TPS)
./build/deptran_server \
  -f config/none_raft.yml \
  -f config/12c1s3r1p.yml \
  -f config/rw.yml \
  -f config/client_closed.yml \
  -f config/concurrent_12.yml \
  -d 30 -m 100 -P localhost

# Raft + Jetpack failover scenario
./build/deptran_server \
  -f config/rule_raft.yml \
  -f config/1c1s3r1p.yml \
  -f config/rw.yml \
  -f config/client_closed.yml \
  -f config/concurrent_1.yml \
  -f config/failover.yml \
  -d 30 -m 100 -P localhost
```

### Raft Lab Harness

```bash
cmake -B build -DRAFT_TEST=ON
cmake --build build -j32
./build/deptran_server -f config/raft_lab_test.yml
```

Watch for `[JETPACK-RECOVERY]` log blocks after leader changes when Jetpack is enabled.

---

## Configuration

Update host maps for distributed runs:

```bash
bash ./src/mako/update_config.sh
```

Key configuration directories:

- Host topology: `config/hosts*.yml`
- Workload knobs: `config/rw.yml`, `config/client_closed.yml`, `config/concurrent_*.yml`
- Paxos protocol: `config/1leader_2followers/*.yml`, `config/occ_paxos.yml`
- Raft protocol: `config/none_raft.yml`, `config/rule_raft.yml`, `config/raft_lab_test.yml`, `config/1c1s3r1p.yml`

---

## Results Processing

```bash
python3 results_processor.py <results/timestamp>
# Example:
python3 results_processor.py 2023-10-10-03:38:03
```

---

## Project Structure

```
mako/
├── src/
│   ├── deptran/           # Transaction protocols (Paxos, Raft, Janus, etc.)
│   │   ├── paxos/         # Paxos-based Mako implementation
│   │   ├── raft/          # Raft + Jetpack implementation
│   │   └── ...
│   ├── mako/              # Masstree, benchmarks, watermark logic
│   ├── bench/             # Workloads (TPC-C, TPC-A, RW)
│   ├── rrr/               # Custom RPC framework
│   └── memdb/             # In-memory datastore
├── config/                # YAML configs for hosts, workloads, consensus
├── build/                 # Generated binaries and libraries
├── ci/                    # Continuous integration scripts
├── doc/                   # Detailed design docs (Raft, Mako, migration plan)
└── README.md              # This document
```

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────┐
│                    Client Applications                   │
└──────────────┬───────────────────────────────┬──────────┘
               │                               │
      ┌────────▼────────┐              ┌───────▼────────┐
      │ Transaction     │              │ Watermark &     │
      │ Coordinators    │              │ Recovery Logic  │
      └────────┬────────┘              └────────┬────────┘
               │                               │
      ┌────────▼────────┐              ┌───────▼────────┐
      │ Consensus Layer │◄────────────►│  Masstree /     │
      │  Paxos or Raft  │              │  RocksDB Store  │
      └────────┬────────┘              └────────┬────────┘
               │                               │
      ┌────────▼────────┐              ┌───────▼────────┐
      │ Network (eRPC,  │              │ Disk / Check-  │
      │ TCP, DPDK)      │              │ pointing        │
      └─────────────────┘              └────────────────┘
```

Leaders execute transactions speculatively; consensus replicates logs; followers validate against watermarks before exposing state. Jetpack extends Raft with witness-based fast recovery.

---

## Troubleshooting

- **Frequent Raft leader churn:** Increase heartbeat interval in `config/none_raft.yml` and confirm followers log `resetTimer()` messages.
- **Commands stuck uncommitted:** Check connectivity and `match_index_` in Raft logs or Paxos learner output.
- **Jetpack not triggering:** Ensure `config/rule_raft.yml` is included and watch for `[JETPACK-RECOVERY]` in logs.
- **Build failures after CMake edits:** Always re-run `cmake -B build ...` before invoking `cmake --build` or `make`.

---

Happy hacking! Open an issue with reproduction steps (commands + configs) if you run into trouble, and include leader/follower logs for consensus-related questions.
