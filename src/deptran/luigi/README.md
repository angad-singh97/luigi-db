# Luigi: Timestamp-Ordered Transaction Execution for Mako

Luigi is a distributed transaction protocol that uses **timestamp ordering** instead of optimistic concurrency control (OCC). It is designed to work with Mako's replication infrastructure while using Tiga's storage and coordination model.

## Overview

### Motivation

Mako and Luigi employ fundamentally different transaction execution models:

| Aspect | Mako (STO/OCC) | Luigi (Stored Procedure) |
|--------|----------------|--------------------------|
| **Execution location** | Client computes | Server computes |
| **Round trips** | Multiple (read → modify → write) | Single (send all ops upfront) |
| **Validation** | OCC at commit time | Timestamp ordering |
| **Storage needs** | Read-set tracking, write-set buffering | Simple put/get |

Mako's storage engine (Masstree) is tightly integrated with STO (Software Transactional Objects) for OCC validation. Luigi's timestamp-ordered execution requires simple, non-transactional put/get operations that bypass this validation entirely. Rather than engineering fragile wrappers to disable STO checks, we replaced the storage layer with Tiga's memdb—a simpler key-value store designed for stored-procedure execution.

This architectural decision transformed the project from a protocol adapter into a **hybrid engine** combining:
- **Mako's replication infrastructure** (Paxos, transport layer, configuration)
- **Tiga's storage and coordination model** (memdb, stored procedures)

### Protocol Summary

1. **Timestamp Initialization**: Coordinator assigns future timestamps based on One-Way Delay (OWD) measurements: `T.timestamp = now() + max_OWD + headroom`

2. **Transaction Dispatch**: Complete operation set sent upfront to all involved shards

3. **Timestamp Agreement**: Leaders exchange timestamps; `T.agreed_ts = max(T.timestamp)` across all shards

4. **Execution**: If timestamps agree, leaders execute and replicate via Paxos

5. **Commit**: Coordinator confirms commit when watermarks advance past transaction timestamp

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        Luigi Benchmark                          │
│                     (luigi_bench_main.cc)                       │
├─────────────────────────────────────────────────────────────────┤
│  LuigiBenchmarkClient  │  TxnGenerators (Micro, TPC-C)         │
├─────────────────────────────────────────────────────────────────┤
│                         LuigiClient                             │
│                   (Dispatch requests to shards)                 │
├─────────────────────────────────────────────────────────────────┤
│    LuigiOWD Service    │    Luigi Transport Setup              │
│  (One-way delay est.)  │  (Delegates to Mako's eRPC)           │
├─────────────────────────────────────────────────────────────────┤
│                    Mako Infrastructure                          │
│         (FastTransport, Configuration, BenchmarkConfig)         │
└─────────────────────────────────────────────────────────────────┘
```

## Key Components

| File | Description |
|------|-------------|
| `luigi_bench_main.cc` | Main entry point for benchmark |
| `luigi_benchmark_client.h/cc` | Benchmark runner with statistics |
| `luigi_client.h/cc` | Client for Luigi dispatch requests |
| `luigi_transport_setup.h/cc` | Transport initialization (delegates to Mako) |
| `luigi_owd.h/cc` | One-Way Delay measurement service |
| `luigi_scheduler.h/cc` | Server-side transaction scheduling |
| `luigi_executor.h/cc` | Transaction execution engine |
| `micro_txn_generator.h` | Micro benchmark workload generator |
| `tpcc_txn_generator.h` | TPC-C workload generator |

## Usage

### Building

```bash
cd /path/to/mako
make -j32
# Binary: ./build/luigi_bench
```

### Running Benchmarks

**Using test script (recommended):**
```bash
# TPC-C with 6 threads, 2 shards
bash examples/test_luigi_bench.sh 6 --benchmark tpcc --duration 30

# Micro benchmark with 4 threads
bash examples/test_luigi_bench.sh 4 --benchmark micro --duration 30
```

**Direct invocation (Mako CI compatible):**
```bash
./build/luigi_bench \
    --shard-config src/mako/config/local-shards2-warehouses6.yml \
    --shard-index 0 \
    -P localhost \
    --num-threads 6 \
    --benchmark tpcc \
    --duration 30
```

### Command Line Options

| Option | Description | Default |
|--------|-------------|---------|
| `-q, --shard-config` | Shard configuration YAML file | (required) |
| `-g, --shard-index` | This shard's index | 0 |
| `-P` | Cluster name (localhost, p1, p2, learner) | localhost |
| `-t, --num-threads` | Worker threads | 1 |
| `--benchmark` | Workload type: micro, micro_single, tpcc | tpcc |
| `--duration` | Test duration in seconds | 30 |
| `--keys` | Keys per shard (micro benchmark) | 100000 |
| `--read-ratio` | Read ratio for micro benchmark | 0.5 |

### Configuration

Luigi reuses Mako's configuration system:
- **Shard configs**: `src/mako/config/local-shards{N}-warehouses{W}.yml`
- **Transport selection**: Set `MAKO_TRANSPORT=erpc` or `MAKO_TRANSPORT=rrr`

## Results

### Evaluation Status: 🔄 Pending

The following experiments are planned but not yet completed:

#### Planned Experiments

| Experiment | Configuration | Status |
|------------|---------------|--------|
| **Single-shard throughput** | 1 shard, 1-16 threads, micro | ⏳ Pending |
| **Multi-shard throughput** | 2-8 shards, 6 threads, micro | ⏳ Pending |
| **TPC-C throughput** | 2 shards, 6 threads, TPC-C | ⏳ Pending |
| **Latency distribution** | 2 shards, P50/P99/P99.9 | ⏳ Pending |
| **Comparison vs Mako OCC** | Same workload, both protocols | ⏳ Pending |
| **Replication overhead** | With/without Paxos | ⏳ Pending |

#### Expected Metrics

```
========== Benchmark Results ==========
Duration:          [PENDING] ms
Total Txns:        [PENDING]
Committed:         [PENDING]
Aborted:           [PENDING] ([PENDING]%)
Throughput:        [PENDING] txns/sec
Avg Latency:       [PENDING] us
P50 Latency:       [PENDING] us
P99 Latency:       [PENDING] us
P99.9 Latency:     [PENDING] us
========================================
```

#### Baseline Comparisons (Planned)

| System | Throughput (txns/sec) | P99 Latency (μs) | Notes |
|--------|----------------------|------------------|-------|
| Mako (OCC) | TBD | TBD | Baseline |
| Luigi (Timestamp) | TBD | TBD | This work |
| Tiga (Reference) | TBD | TBD | Original implementation |

### Known Limitations

1. **Storage layer**: Currently uses Tiga's memdb; RocksDB persistence not yet integrated
2. **Replication**: Paxos integration in progress
3. **Multi-datacenter**: OWD measurement assumes single datacenter latencies

## References

- [LUIGI_PROTOCOL.md](LUIGI_PROTOCOL.md) - Detailed protocol specification
- Mako: [../../mako/](../../mako/) - Parent system documentation
- Tiga: Original stored-procedure transaction system

## Contact

For questions about Luigi integration with Mako, see the main repository documentation.
