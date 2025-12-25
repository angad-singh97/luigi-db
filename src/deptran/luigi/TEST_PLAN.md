# Luigi Test Plan

## Overview

This document outlines the comprehensive testing strategy for Luigi, including:
1. Performance characterization experiments
2. Test scripts and automation
3. Result collection and visualization
4. WAN simulation setup

## Test Infrastructure

### Test Directory Structure

```
src/deptran/luigi/test/
├── scripts/
│   ├── run_network_latency_sweep.sh
│   ├── run_cross_shard_sweep.sh
│   ├── run_worker_count_sweep.sh
│   ├── run_microbench_suite.sh
│   ├── run_tpcc_suite.sh
│   └── setup_wan_simulation.sh
├── configs/
│   ├── 1shard-norep.yml
│   ├── 1shard-rep.yml
│   ├── 2shard-norep.yml
│   └── 2shard-rep.yml
└── results/
    ├── network_latency/
    ├── cross_shard/
    ├── worker_count/
    ├── microbench/
    └── tpcc/
```

---

## Experiment 1: Network Latency Impact

**Objective:** Measure how network latency affects throughput and latency

**Configuration:**
- 2 shards with replication (3 replicas per shard)
- Fixed worker count (optimal from Experiment 3)
- Variable network latency: 0ms, 10ms, 25ms, 50ms, 100ms, 200ms

**Metrics:**
- Throughput (txns/sec)
- Average latency (ms)
- P50, P95, P99 latency (ms)

**Expected Results Table:**

| Network Latency | Throughput (txns/sec) | Avg Latency (ms) | P50 (ms) | P95 (ms) | P99 (ms) |
|-----------------|----------------------|------------------|----------|----------|----------|
| 0ms (LAN)       |                      |                  |          |          |          |
| 10ms            |                      |                  |          |          |          |
| 25ms            |                      |                  |          |          |          |
| 50ms            |                      |                  |          |          |          |
| 100ms (WAN)     |                      |                  |          |          |          |
| 200ms           |                      |                  |          |          |          |

**Script:** `test/scripts/run_network_latency_sweep.sh`

---

## Experiment 2: Cross-Shard Transaction Ratio

**Objective:** Measure impact of cross-shard transactions on performance

**Configuration:**
- 2 shards with replication (3 replicas per shard)
- Fixed worker count (optimal from Experiment 3)
- Variable cross-shard %: 0%, 10%, 25%, 50%, 75%, 100%

**Metrics:**
- Throughput (txns/sec)
- Average latency (ms)
- Commit rate (%)

**Expected Results Table:**

| Cross-Shard % | Throughput (txns/sec) | Avg Latency (ms) | Commit Rate (%) |
|---------------|----------------------|------------------|-----------------|
| 0%            |                      |                  |                 |
| 10%           |                      |                  |                 |
| 25%           |                      |                  |                 |
| 50%           |                      |                  |                 |
| 75%           |                      |                  |                 |
| 100%          |                      |                  |                 |

**Script:** `test/scripts/run_cross_shard_sweep.sh`

---

## Experiment 3: Worker Count Optimization

**Objective:** Find optimal coordinator worker thread count

**Configuration:**
- 2 shards with replication (3 replicas per shard)
- Variable worker count: 1, 2, 4, 8, 16, 32

**Metrics:**
- Throughput (txns/sec)
- CPU utilization (%)
- Average latency (ms)

**Expected Results Table:**

| Worker Count | Throughput (txns/sec) | CPU Util (%) | Avg Latency (ms) |
|--------------|----------------------|--------------|------------------|
| 1            |                      |              |                  |
| 2            |                      |              |                  |
| 4            |                      |              |                  |
| 8            |                      |              |                  |
| 16           |                      |              |                  |
| 32           |                      |              |                  |

**Script:** `test/scripts/run_worker_count_sweep.sh`

---

## Experiment 4: Microbenchmark Suite

**Objective:** Baseline performance across different configurations

**Test Matrix:**

| Configuration | Shards | Replication | Expected Throughput |
|---------------|--------|-------------|---------------------|
| Config 1      | 1      | No          | ~15K txns/sec       |
| Config 2      | 2      | No          | ~30K txns/sec       |
| Config 3      | 1      | Yes (3x)    | ~8K txns/sec        |
| Config 4      | 2      | Yes (3x)    | ~16K txns/sec       |

**Detailed Results Template:**

### Config 1: Single-Shard, No Replication
```
Throughput:     _____ txns/sec
Avg Latency:    _____ ms
P50 Latency:    _____ ms
P95 Latency:    _____ ms
P99 Latency:    _____ ms
Commit Rate:    _____ %
```

### Config 2: 2-Shard, No Replication
```
Throughput:     _____ txns/sec
Avg Latency:    _____ ms
P50 Latency:    _____ ms
P95 Latency:    _____ ms
P99 Latency:    _____ ms
Commit Rate:    _____ %
```

### Config 3: Single-Shard, Replication (3x)
```
Throughput:     _____ txns/sec
Avg Latency:    _____ ms
P50 Latency:    _____ ms
P95 Latency:    _____ ms
P99 Latency:    _____ ms
Commit Rate:    _____ %
```

### Config 4: 2-Shard, Replication (3x)
```
Throughput:     _____ txns/sec
Avg Latency:    _____ ms
P50 Latency:    _____ ms
P95 Latency:    _____ ms
P99 Latency:    _____ ms
Commit Rate:    _____ %
```

**Script:** `test/scripts/run_microbench_suite.sh`

---

## Experiment 5: TPC-C Benchmark Suite

**Objective:** Real-world workload performance

**Test Matrix:**

| Configuration | Shards | Replication | Expected Throughput |
|---------------|--------|-------------|---------------------|
| Config 1      | 1      | No          |                     |
| Config 2      | 2      | No          |                     |
| Config 3      | 1      | Yes (3x)    |                     |
| Config 4      | 2      | Yes (3x)    |                     |

**Detailed Results Template:**

### Config 1: Single-Shard, No Replication
```
Throughput:     _____ txns/sec
Avg Latency:    _____ ms
P50 Latency:    _____ ms
P95 Latency:    _____ ms
P99 Latency:    _____ ms
Commit Rate:    _____ %
NewOrder %:     _____ %
Payment %:      _____ %
```

### Config 2: 2-Shard, No Replication
```
Throughput:     _____ txns/sec
Avg Latency:    _____ ms
P50 Latency:    _____ ms
P95 Latency:    _____ ms
P99 Latency:    _____ ms
Commit Rate:    _____ %
NewOrder %:     _____ %
Payment %:      _____ %
```

### Config 3: Single-Shard, Replication (3x)
```
Throughput:     _____ txns/sec
Avg Latency:    _____ ms
P50 Latency:    _____ ms
P95 Latency:    _____ ms
P99 Latency:    _____ ms
Commit Rate:    _____ %
NewOrder %:     _____ %
Payment %:      _____ %
```

### Config 4: 2-Shard, Replication (3x)
```
Throughput:     _____ txns/sec
Avg Latency:    _____ ms
P50 Latency:    _____ ms
P95 Latency:    _____ ms
P99 Latency:    _____ ms
Commit Rate:    _____ %
NewOrder %:     _____ %
Payment %:      _____ %
```

**Script:** `test/scripts/run_tpcc_suite.sh`

---

## WAN Simulation Setup

### Network Emulation with tc (netem)

**Configuration:**
- Delay: 100ms
- Jitter: 20ms
- Distribution: Pareto

**Setup Script:** `test/scripts/setup_wan_simulation.sh`

**Commands:**
```bash
# Add 100ms delay with 20ms jitter (Pareto distribution)
sudo tc qdisc add dev lo root netem delay 100ms 20ms distribution pareto

# Verify
sudo tc qdisc show dev lo

# Remove (cleanup)
sudo tc qdisc del dev lo root
```

**Testing with WAN simulation:**
1. Run `setup_wan_simulation.sh` to enable
2. Execute test scripts
3. Run `cleanup_wan_simulation.sh` to disable
4. Compare results with/without WAN simulation

---

## Result Visualization

### Graph Templates

All experiments will generate:
1. **CSV files** with raw data
2. **PNG graphs** using gnuplot or matplotlib
3. **Summary tables** in markdown format

### Example Graph: Network Latency vs Throughput

```
Throughput (txns/sec)
    ^
15K |     *
    |
10K |        *
    |
 5K |           *
    |              *
  0 +--*-----------*---------> Network Latency (ms)
    0   10  25  50  100  200
```

### Example Graph: Cross-Shard % vs Throughput

```
Throughput (txns/sec)
    ^
15K |  *
    |
10K |     *
    |        *
 5K |           *
    |              *
  0 +------------------*------> Cross-Shard %
    0   25   50   75  100
```

---

## Test Execution Order

1. **Worker Count Optimization** (Experiment 3)
   - Determine optimal worker count for subsequent tests

2. **Microbench Suite** (Experiment 4)
   - Establish baseline performance

3. **Network Latency Sweep** (Experiment 1)
   - Without WAN simulation (LAN baseline)
   - With WAN simulation (100ms + 20ms jitter)

4. **Cross-Shard Sweep** (Experiment 2)
   - Test scalability with distributed transactions

5. **TPC-C Suite** (Experiment 5)
   - Real-world workload validation

---

## Automation

### Master Test Script

Run all experiments:
```bash
cd src/deptran/luigi/test
./scripts/run_all_tests.sh
```

This will:
1. Create result directories
2. Run all experiments in order
3. Generate graphs and tables
4. Create summary report in `results/SUMMARY.md`

### Individual Test Execution

```bash
# Network latency sweep
./scripts/run_network_latency_sweep.sh

# Cross-shard sweep
./scripts/run_cross_shard_sweep.sh

# Worker count optimization
./scripts/run_worker_count_sweep.sh

# Microbench suite
./scripts/run_microbench_suite.sh

# TPC-C suite
./scripts/run_tpcc_suite.sh
```

---

## Expected Deliverables

1. **Test Scripts** - All automation in `test/scripts/`
2. **Configuration Files** - YAML configs in `test/configs/`
3. **Raw Results** - CSV files in `test/results/`
4. **Graphs** - PNG visualizations
5. **Summary Report** - `test/results/SUMMARY.md`
6. **Analysis** - Performance insights and recommendations
