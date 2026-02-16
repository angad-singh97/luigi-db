# Mako + Luigi

<div align="center">

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![OSDI'25](https://img.shields.io/badge/OSDI'25-Mako-orange.svg)](#mako-base-system)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)

**Distributed Transactional Systems: Mako (OSDI'25) + Luigi (Timestamp-Ordered Protocol)**

[Luigi — What We Built](#luigi--what-we-built) • [Quick Start](#quick-start) • [Luigi vs Mako](#luigi-vs-mako-benchmarks) • [Mako Base System](#mako-base-system)

</div>

---

## Luigi — What We Built

**Luigi** is a distributed transaction protocol that uses **timestamp-ordered execution** to achieve high throughput in geo-distributed environments. It is inspired by the Tiga protocol (SOSP 2025) and designed to compare against OCC-based systems like Mako.

Luigi lives in `src/deptran/luigi/` — coordinator, server, scheduler, executor, state machine, and RPC layer. The base Mako system (Shuai Mu et al., OSDI'25) provides the infrastructure; we implemented the Luigi protocol on top.

### Luigi Protocol Summary

| Phase | Description |
|-------|-------------|
| **Timestamp Init** | Coordinator assigns future timestamps: `T.timestamp = now() + max_OWD + headroom` |
| **Receipt & Queue** | Leaders do conflict detection, insert into timestamp-ordered priority queue |
| **Timestamp Agreement** | Leaders exchange timestamps; `T.agreed_ts = max(T.timestamp)` across shards |
| **Execution & Replication** | Execute transaction, append to Paxos stream |
| **Watermark & Commit** | Coordinator replies when `T.timestamp <= watermark[shard][worker_ID]` for all shards |

For full protocol spec, message formats, and RPC definitions, see **[src/deptran/luigi/LUIGI_PROTOCOL.md](src/deptran/luigi/LUIGI_PROTOCOL.md)**.

### Luigi Highlights

| Metric | Value |
|--------|-------|
| **2-WRTT latency bound** | Commit latency ≤ 2 Wide-Area Round Trip Times |
| **Multi-shard vs Mako** | **30–60× higher throughput** in 2-shard, geo-distributed setups |
| **Cross-shard study** | 60–160× advantage as cross-shard % increases (TPC-C) |

**When Luigi wins:** Multi-shard workloads, high-latency networks. **When Mako wins:** Single-shard, low-latency networks (3–10× higher throughput).

---

## Quick Start

### Prerequisites

- CMake 3.10+, C++17, Linux with `tc` (traffic control) for network simulation  
- Tested on **Debian 12** and **Ubuntu 22.04**

### Build

```bash
# Clone and enter project
git clone --recursive https://github.com/angad-singh97/luigi-db.git
cd mako

# Install dependencies
bash apt_packages.sh
source install_rustc.sh
bash src/mako/update_config.sh

# Build (produces luigi_server, luigi_coordinator, dbtest)
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

### Run Luigi Benchmarks

**TPC-C, 2-shard, 3-replicas:**
```bash
./src/deptran/luigi/test/scripts/luigi/run_tpcc_2shard_3replicas.sh <duration> <threads>
```

**With network latency simulation:**
```bash
# Parameters: duration threads owd_ms headroom_ms netem_delay_ms netem_jitter_ms
./src/deptran/luigi/test/scripts/luigi/run_tpcc_2shard_3replicas_latency.sh 30 8 160 30 150 20
```

**Run all Luigi benchmarks:**
```bash
./src/deptran/luigi/test/scripts/luigi/run_all_benchmarks.sh
```

**Run Mako benchmarks (for comparison):**
```bash
./src/deptran/luigi/test/scripts/mako/run_mako_tpcc_2shard_3replicas.sh <duration> <threads>
./src/deptran/luigi/test/scripts/mako/run_all_mako_tpcc_benchmarks.sh
```

Full Luigi docs: **[src/deptran/luigi/README.md](src/deptran/luigi/README.md)**

---

## Luigi vs Mako Benchmarks

TPC-C benchmark on cloud infrastructure (8 cores, 16 GB RAM, Ubuntu 22.04). Network simulated with `tc netem`.

| Configuration | Winner | Advantage |
|---------------|--------|-----------|
| 1-shard, 1-replica | Mako | 3–10× higher throughput |
| 1-shard, 3-replicas | Mako | 3–50× higher throughput |
| **2-shard, 1-replica** | **Luigi** | **30–60× higher throughput** |
| **2-shard, 3-replicas** | **Luigi** | **30–60× higher throughput** |

**Cross-shard study (2-shard, 3-replica, geo-distributed):** Luigi maintains 2,500+ TPS; Mako drops to 17–43 TPS as cross-shard % increases → **60–160× Luigi advantage**.

Luigi’s 2-WRTT design (~600 ms) vs Mako’s 5+ round trips (1,440–1,680 ms for cross-shard) explains the gap. See [src/deptran/luigi/README.md](src/deptran/luigi/README.md) for full results.

---

## Mako Base System

*The base distributed transactional key-value store (Shuai Mu et al., [OSDI'25](https://www.usenix.org/conference/osdi25/presentation/shen-weihai)) provides the infrastructure. Luigi is our protocol built on top.*

**Mako** decouples transaction execution from replication using a speculative 2PC protocol. Transactions run at full speed locally while replication happens asynchronously, achieving fault-tolerance without blocking on cross-datacenter consensus. Processing **3.66M TPC-C transactions per second** with 10 shards replicated across the continent; **8.6× higher throughput** than state-of-the-art geo-replicated systems.

---

## Replication Layers

Mako supports **two replication backends** that can be selected at build time:

| Replication | Build Command | Binary | Use Case |
|-------------|---------------|--------|----------|
| **Paxos** (default) | `make -j32` | `dbtest` | Production Mako with Paxos consensus |
| **Raft** | `make mako-raft -j64` | `dbtest` | Mako with Raft as replication layer |

Additionally, **Raft can run standalone** (without Mako) via `deptran_server`:

| Mode | Build Command | Binary | Use Case |
|------|---------------|--------|----------|
| **Standalone Raft** | `make -j32` or `make mako-raft -j64` | `deptran_server` | Raft consensus without Mako transactions |
| **Raft Lab Tests** | `make raft-test -j32` | `deptran_server` | Raft coroutine-based lab test suite only |

> **Note**: `make raft-test` enables `RAFT_TEST` coroutines for the lab test harness. This mode is **only for running `config/raft_lab_test.yml`** - the normal concurrency configs (like `12c1s3r1p.yml`) won't work with this build.

### Understanding the Difference

- **Mako + Paxos**: Original Mako system using Paxos for replication (`./ci/ci.sh`)
- **Mako + Raft**: Mako transactions with Raft as the replication layer (`./ci/ci_mako_raft.sh`)
- **Standalone Raft**: Pure Raft consensus testing via `deptran_server` (no Mako transactions) - use regular `make` build
- **Raft Lab Tests**: Coroutine-based Raft tests (`make raft-test`) - only for `raft_lab_test.yml`

---

## Why Choose Mako?

### Proven Research & Performance
- Backed by peer-reviewed research published at OSDI'25, one of the top-tier systems conferences
- **8.6× higher throughput** than state-of-the-art geo-replicated systems
- Processing **3.66M TPC-C transactions per second** with geo-replication

### Core Capabilities
- **Serializable Transactions**: Strongest isolation level with full ACID guarantees across distributed partitions
- **Geo-Replication**: Multi-datacenter support with configurable consistency for disaster recovery
- **Pluggable Replication**: Choose between Paxos or Raft consensus protocols
- **High-Performance Storage**: Built on **Masstree** for in-memory indexing; RocksDB backend for persistence
- **Horizontal Scalability**: Automatic sharding and data partitioning across nodes
- **Fault Tolerance**: Crash recovery and replication for high availability
- **Advanced Networking**: DPDK support for kernel bypass and ultra-low latency
- **Rust-like memory safety** by using RustyCpp for borrow checking and lifetime analysis

### Developer-Friendly
- **Industry-standard benchmarks**: TPC-C, TPC-A, read-write workloads, and micro-benchmarks
- **RocksDB-like interface** for easy migration from single-node deployments
- **Redis-compatible layer** for familiar API with enhanced consistency
- Comprehensive test suite
- Modular architecture for extensions

---

## Build System

### Build Targets

| Target | Command | Description |
|--------|---------|-------------|
| **Mako + Paxos** | `make -j32` | Default build with Paxos replication (~2-3 mins) |
| **Mako + Raft** | `make mako-raft -j32` | Mako with Raft replication (builds `deptran_server` and Raft test binaries) |
| **Raft Lab Tests** | `make raft-test -j32` | Raft with testing coroutines (only for `raft_lab_test.yml`) |
| **Clean** | `make clean` | Remove all build artifacts |
| **Help** | `make help` | Show all available targets |

### Output Binaries

| Binary | Build | Description |
|--------|-------|-------------|
| `build/luigi_server` | cmake | Luigi server (runs on each replica) |
| `build/luigi_coordinator` | cmake | Luigi coordinator/client (generates transactions) |
| `build/dbtest` | all | Main Mako binary (works with both Paxos and Raft replication) |
| `build/deptran_server` | mako-raft | Standalone Raft server (for Raft-only testing) |
| `build/simpleRaft` | mako-raft | Simple Raft replication test |
| `build/simpleTransactionRepRaft` | mako-raft | Raft-based transaction replication test |
| `build/testPreferredReplicaStartup` | mako-raft | Raft preferred replica startup test |
| `build/testPreferredReplicaLogReplication` | mako-raft | Raft log replication test |
| `build/testNoOps` | mako-raft | Raft no-op test |

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `MAKO_USE_RAFT` | `OFF` | Use `raft_main_helper.cc`, build Raft executables, define `MAKO_USE_RAFT=1` |
| `RAFT_TEST` | `OFF` | Define `RAFT_TEST_CORO=1` and `REUSE_CORO=1` for lab test coroutines |
| `ENABLE_BORROW_CHECKING` | `OFF` | Enable RustyCpp borrow checking |
| `DEBUG` | `OFF` | Enable debug mode with `-DDEBUG` flag |

---

## Running Tests

### Mako + Paxos Tests

Use `./ci/ci.sh` for testing Mako with **Paxos** replication:

```bash
# Run all Paxos CI tests
./ci/ci.sh all

# Individual tests
./ci/ci.sh simpleTransaction       # Simple transactions
./ci/ci.sh simplePaxos             # Paxos replication
./ci/ci.sh shard1Replication       # 1-shard with replication
./ci/ci.sh shard2Replication       # 2-shards with replication
./ci/ci.sh shard1ReplicationSimple
./ci/ci.sh shard2ReplicationSimple
./ci/ci.sh rocksdbTests            # RocksDB persistence
./ci/ci.sh shardFaultTolerance     # Fault tolerance
./ci/ci.sh multiShardSingleProcess
./ci/ci.sh cpuThrottlingScaling
```

### Mako + Raft Tests

Use `./ci/ci_mako_raft.sh` for testing Mako with **Raft** replication:

```bash
# Build Mako with Raft first
make mako-raft -j64

# Run all Mako-Raft CI tests
./ci/ci_mako_raft.sh all

# Individual tests
./ci/ci_mako_raft.sh compile                    # Build with Raft
./ci/ci_mako_raft.sh simpleRaft                 # Simple Raft replication
./ci/ci_mako_raft.sh shard1ReplicationRaft      # 1-shard Raft
./ci/ci_mako_raft.sh shard2ReplicationRaft      # 2-shard Raft
./ci/ci_mako_raft.sh shard1ReplicationSimpleRaft
./ci/ci_mako_raft.sh shard2ReplicationSimpleRaft
./ci/ci_mako_raft.sh cleanup                    # Clean up processes
```

### Standalone Raft Tests (No Mako)

Use `deptran_server` for testing **Raft consensus only** (without Mako transactions).

#### Running Standalone Raft (use regular build)

```bash
# Build (regular make, NOT raft-test)
make -j32

# Basic Raft (1 client, 1 shard, 3 replicas)
./build/deptran_server \
  -f config/none_raft.yml \
  -f config/1c1s3r1p.yml \
  -f config/rw.yml \
  -f config/client_closed.yml \
  -f config/concurrent_1.yml \
  -d 30 -m 100 -P localhost

# Higher concurrency (12 clients, ~25k TPS)
./build/deptran_server \
  -f config/none_raft.yml \
  -f config/12c1s3r1p.yml \
  -f config/rw.yml \
  -f config/client_closed.yml \
  -f config/concurrent_12.yml \
  -d 30 -m 100 -P localhost
```

#### Raft Lab Tests (use raft-test build)

```bash
# Build with RAFT_TEST coroutines (only for lab tests)
make raft-test -j32

# Run Raft lab test suite
./build/deptran_server -f config/raft_lab_test.yml
```

> **Warning**: The `make raft-test` build enables special coroutines for the lab harness. The normal concurrency configs (`1c1s3r1p.yml`, `12c1s3r1p.yml`, etc.) will **not work** with this build.

### Unit Tests

```bash
# CTest integration
make test                 # Run all tests
make test-verbose         # Verbose output
make test-parallel        # Parallel execution

# Silo/STO unit tests
cd tests && ./run_tests.sh all
```

---

## Running Mako with Paxos

### Single Machine Setup (1 Leader + 2 Followers + 1 Learner)

```bash
# Build
make -j32

# Start followers and leader in separate terminals
# Follower p1
./build/dbtest --verbose --bench tpcc --basedir ./tmp \
  --db-type mbta --num-threads 6 --scale-factor 6 \
  -F config/1leader_2followers/paxos6_shardidx0.yml -F config/occ_paxos.yml \
  --txn-flags 1 --runtime 30 -P p1 &

# Follower p2
./build/dbtest --verbose --bench tpcc --basedir ./tmp \
  --db-type mbta --num-threads 6 --scale-factor 6 \
  -F config/1leader_2followers/paxos6_shardidx0.yml -F config/occ_paxos.yml \
  --txn-flags 1 --runtime 30 -P p2 &

# Leader
./build/dbtest --verbose --bench tpcc --basedir ./tmp \
  --db-type mbta --num-threads 6 --scale-factor 6 \
  -F config/1leader_2followers/paxos6_shardidx0.yml -F config/occ_paxos.yml \
  --txn-flags 1 --runtime 30 -P localhost &

# Monitor logs
tail -f leader.log p1.log p2.log
```

---

## Running Mako with Raft

### Build and Test

```bash
# Build Mako with Raft replication
make mako-raft -j64

# Run the Mako-Raft CI suite
./ci/ci_mako_raft.sh all

# Or run individual tests
./ci/ci_mako_raft.sh simpleRaft
./ci/ci_mako_raft.sh shard1ReplicationRaft
```

The `dbtest` binary with Raft replication runs Mako transactions but uses Raft (instead of Paxos) for log replication and leader election.

---

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    Client Applications                   │
└─────────────────────┬───────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────┐
│              Transaction Coordinators                    │
│  ┌──────────┬──────────┬──────────┬──────────┐         │
│  │  Mako    │   2PL    │   OCC    │  Janus   │         │
│  └──────────┴──────────┴──────────┴──────────┘         │
└─────────────────────┬───────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────┐
│            Replication Layer (Pluggable)                 │
│         ┌──────────────┬──────────────┐                 │
│         │    Paxos     │     Raft     │                 │
│         └──────────────┴──────────────┘                 │
└─────────────────────┬───────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────┐
│                RPC Communication Layer                   │
│           (TCP/IP, DPDK, RDMA, eRPC)                    │
└─────────────────────┬───────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────┐
│              Sharded Data Partitions                     │
│  ┌─────────────┬─────────────┬─────────────┐           │
│  │   Shard 1   │   Shard 2   │   Shard N   │           │
│  │  (Replicas) │  (Replicas) │  (Replicas) │           │
│  └─────────────┴─────────────┴─────────────┘           │
└─────────────────────┬───────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────┐
│              Storage Backends                            │
│    Masstree (In-Memory)  |  RocksDB (Persistent)        │
└─────────────────────────────────────────────────────────┘
```

---

## Benchmarks

Performance results from our OSDI'25 evaluation on Azure cloud infrastructure (TPC-C benchmark):

| Configuration | Shards | Threads/Shard | Throughput | Notes |
|--------------|--------|---------------|------------|-------|
| Single Shard | 1 | 24 | 960K TPS | 22.5× faster than Calvin |
| Geo-Replicated | 10 | 24 | 3.66M TPS | 8.6× faster than Calvin |

### Performance Advantages

- **8.6× higher throughput** than Calvin (state-of-the-art geo-replicated system)
- **22.5× higher throughput** than Calvin at single shard
- **32.2× higher throughput** than OCC+OR at 10 shards

---

## Configuration

Update host maps for distributed runs:

```bash
bash ./src/mako/update_config.sh
```

Key configuration directories:

| Directory | Purpose |
|-----------|---------|
| `config/hosts*.yml` | Host topology |
| `config/rw.yml`, `config/concurrent_*.yml` | Workload settings |
| `config/occ_paxos.yml`, `config/1leader_2followers/` | Paxos protocol |
| `config/none_raft.yml`, `config/rule_raft.yml` | Raft protocol |

---

## Project Structure

```
mako/
├── src/
│   ├── deptran/           # Transaction protocols
│   │   ├── luigi/         # Luigi protocol (timestamp-ordered, our contribution)
│   │   │   ├── coordinator.cc/h, server.cc/h, scheduler.cc/h, executor.cc/h
│   │   │   ├── state_machine.cc/h, commo.cc/h, service.cc/h
│   │   │   ├── test/scripts/luigi/   # Luigi benchmark scripts
│   │   │   └── test/scripts/mako/    # Mako benchmark scripts (for comparison)
│   │   ├── paxos/         # Paxos replication
│   │   ├── raft/          # Raft replication
│   │   └── ...
│   ├── mako/              # Mako core (Masstree, watermarks)
│   ├── bench/             # Benchmarks (TPC-C, TPC-A, RW)
│   ├── rrr/               # RPC framework
│   └── memdb/             # In-memory datastore
├── config/                # YAML configurations
├── ci/
│   ├── ci.sh              # Mako + Paxos tests
│   └── ci_mako_raft.sh    # Mako + Raft tests
├── examples/
│   └── mako-raft-tests/   # Mako-Raft test scripts
├── tests/                 # Unit tests
├── third-party/           # Dependencies
└── rust-lib/              # Rust components
```

---

## Use Cases

### Distributed RocksDB Alternative

Mako provides a familiar key-value API with distributed transactions, geo-replication, and fault tolerance. Perfect for applications that need:
- **Horizontal scalability** across multiple nodes
- **ACID transactions** spanning multiple keys or partitions
- **Geographic replication** for disaster recovery

### Redis Alternative with Transactions

Mako includes a Redis-compatible layer for:
- **Strong consistency** with serializable transactions
- **Multi-key atomic operations** with full ACID guarantees
- **Geographic distribution** with automatic failover

---

## Troubleshooting

| Issue | Solution |
|-------|----------|
| Frequent Raft leader churn | Increase heartbeat interval in `config/none_raft.yml` |
| Commands stuck uncommitted | Check connectivity and `match_index_` in logs |
| Build failures after CMake edits | Re-run `cmake -B build ...` before building |
| Hanging test processes | Run `./ci/ci_mako_raft.sh cleanup` |

---

## Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Make your changes with tests
4. Ensure all tests pass (`make test`)
5. Submit a pull request

---

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

## Acknowledgments

- **Mako**: Shuai Mu et al., [OSDI'25](https://www.usenix.org/conference/osdi25/presentation/shen-weihai)
- **Luigi**: Timestamp-ordered protocol in `src/deptran/luigi/` (our contribution)
- **Dependencies**: Built on Janus, Masstree, RocksDB, eRPC, and other open-source projects
