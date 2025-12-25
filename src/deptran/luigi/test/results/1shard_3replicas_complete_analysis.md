# Luigi 1-Shard 3-Replica Network Latency Benchmark - Complete Analysis

## Executive Summary

Comprehensive performance evaluation of Luigi's 1-shard 3-replica configuration across four network conditions (Same Metro, Same Continent, Cross-Continent, Cross-Region) and four thread counts (1, 2, 4, 8).

**Key Finding**: Optimal thread count varies significantly with network latency. Low-latency scenarios peak at 1 thread, while high-latency scenarios scale effectively through 8 threads.

---

## Complete Results Matrix

| Network Condition | 1 Thread | 2 Threads | 4 Threads | 8 Threads | Peak |
|-------------------|----------|-----------|-----------|-----------|------|
| **Same Metro** (2ms ± 0.5ms) | **10,429** | 8,420 | 6,884 | 5,235 | 1T |
| **Same Continent** (30ms ± 5ms) | 2,811 | 4,797 | **5,467** | 4,701 | 4T |
| **Cross-Continent** (80ms ± 10ms) | 1,133 | 2,185 | 3,290 | **4,521** | 8T |
| **Cross-Region** (150ms ± 20ms) | 614 | 1,190 | 2,188 | **3,261** | 8T |

*All values in txns/sec*

---

## Scaling Analysis

### Same Metro (2ms ± 0.5ms)
- **Peak**: 1 thread at 10,429 txns/sec
- **Scaling**: Negative scaling with more threads (0.81x → 0.82x → 0.76x)
- **Reason**: Coordination overhead dominates in low-latency environments

### Same Continent (30ms ± 5ms)
- **Peak**: 4 threads at 5,467 txns/sec
- **Scaling**: Strong 1T→2T (1.71x), moderate 2T→4T (1.14x), negative 4T→8T (0.86x)
- **Reason**: Optimal balance between concurrency benefits and coordination overhead

### Cross-Continent (80ms ± 10ms)
- **Peak**: 8 threads at 4,521 txns/sec
- **Scaling**: Consistent positive scaling (1.93x → 1.51x → 1.37x)
- **Reason**: Network latency dominates, allowing effective parallelization

### Cross-Region (150ms ± 20ms)
- **Peak**: 8 threads at 3,261 txns/sec
- **Scaling**: Strong positive scaling (1.94x → 1.84x → 1.49x)
- **Total improvement**: 5.3x from 1T to 8T
- **Reason**: Very high network latency makes concurrency highly beneficial

---

## Latency Analysis

### Average Latency by Thread Count

| Network | 1T | 2T | 4T | 8T |
|---------|----|----|----|----|
| Same Metro | 18.4ms | 47.0ms | 115.3ms | 303.5ms |
| Same Continent | 70.3ms | 82.8ms | 145.3ms | 336.5ms |
| Cross-Continent | 174.2ms | 181.1ms | 241.1ms | 347.1ms |
| Cross-Region | 321.2ms | 331.0ms | 363.5ms | 478.7ms |

**Observation**: Latency increases with thread count due to higher concurrent load and contention, with the most dramatic increases in low-latency scenarios.

---

## Tail Latency (P99)

| Network | 1T | 2T | 4T | 8T |
|---------|----|----|----|----|
| Same Metro | 39.9ms | 100.1ms | 206.6ms | 807.5ms |
| Same Continent | 108.1ms | 141.3ms | 247.6ms | 529.7ms |
| Cross-Continent | 254.5ms | 298.0ms | 423.4ms | 543.3ms |
| Cross-Region | 424.1ms | 466.0ms | 492.6ms | 940.5ms |

**Observation**: Tail latencies increase significantly with thread count, especially for low-latency networks where contention is more pronounced.

---

## Recommendations

### For Low-Latency Deployments (< 10ms)
- **Use 1-2 threads** for optimal throughput
- Expect ~10K txns/sec with single thread
- Avoid high thread counts due to coordination overhead

### For Medium-Latency Deployments (10-50ms)
- **Use 4 threads** for optimal balance
- Expect ~5-6K txns/sec
- Sweet spot between concurrency and overhead

### For High-Latency Deployments (50-100ms)
- **Use 6-8 threads** for best throughput
- Expect ~4-5K txns/sec
- Concurrency effectively masks network latency

### For Very High-Latency Deployments (> 100ms)
- **Use 8+ threads** to maximize throughput
- Expect ~3-4K txns/sec
- High concurrency essential for acceptable performance

---

## System Stability

- **Zero aborts** across all 16 test configurations (4 networks × 4 thread counts)
- **100% commit rate** demonstrates robust consensus implementation
- **Consistent behavior** across varying network conditions

---

## Test Configuration

- **Duration**: 10 seconds per test
- **Benchmark**: Micro (read-only transactions)
- **Topology**: 1 shard, 3 replicas
- **Total tests**: 16 (4 network conditions × 4 thread counts)
- **Total transactions**: 659,851 committed

---

## Files Generated

### Individual Results
- Thread 1: `1shard_3replicas_t1_{metro,continent,crosscontinent,crossregion}.txt`
- Thread 2: `1shard_3replicas_t2_{metro,continent,crosscontinent,crossregion}.txt`
- Thread 4: `1shard_3replicas_t4_{metro,continent,crosscontinent,crossregion}.txt`
- Thread 8: `1shard_3replicas_t8_{metro,continent,crosscontinent,crossregion}.txt`

### Summaries
- `1shard_3replicas_t1_summary.md`
- `1shard_3replicas_t2_summary.md`
- `1shard_3replicas_t4_summary.md`
- `1shard_3replicas_t8_summary.md`
- `1shard_3replicas_complete_analysis.md` (this file)
