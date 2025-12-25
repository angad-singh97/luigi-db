# Luigi 1-Shard 3-Replica Microbenchmark Results

## Thread Count: 2

### Test Configuration
- **Duration**: 10 seconds
- **Benchmark**: Micro (read-only transactions)
- **Topology**: 1 shard, 3 replicas

---

## Results Summary

| Network Condition | Latency | OWD | Headroom | Throughput | Avg Latency | P50 Latency | P99 Latency | P99.9 Latency |
|------------------|---------|-----|----------|------------|-------------|-------------|-------------|---------------|
| **Same Metro** | 2ms ± 0.5ms | 5ms | 2ms | 8,420 txns/sec | 47.0 ms | 43.4 ms | 100.1 ms | 164.4 ms |
| **Same Continent** | 30ms ± 5ms | 40ms | 10ms | 4,797 txns/sec | 82.8 ms | 80.1 ms | 141.3 ms | 176.0 ms |
| **Cross-Continent** | 80ms ± 10ms | 100ms | 20ms | 2,185 txns/sec | 181.1 ms | 175.4 ms | 298.0 ms | 303.1 ms |
| **Cross-Region** | 150ms ± 20ms | 180ms | 30ms | 1,190 txns/sec | 331.0 ms | 319.8 ms | 466.0 ms | 702.2 ms |

---

## Key Observations

1. **Throughput comparison with 1 thread**:
   - Same Metro: 8,420 vs 10,429 txns/sec (19% reduction)
   - Same Continent: 4,797 vs 2,811 txns/sec (71% increase)
   - Cross-Continent: 2,185 vs 1,133 txns/sec (93% increase)
   - Cross-Region: 1,190 vs 614 txns/sec (94% increase)

2. **Latency impact**: Average latency increased with 2 threads due to concurrent load, especially noticeable in low-latency scenarios

3. **Concurrency benefits**: Higher network latency scenarios benefit more from concurrency (2 threads nearly doubles throughput for high-latency cases)

4. **Zero aborts**: All tests completed with 0% abort rate, demonstrating system stability under concurrent load

---

## Detailed Results

### Same Metro (2ms ± 0.5ms)
```
Duration:          10017 ms
Total Txns:        84344
Committed:         84344
Aborted:           0 (0.00%)
Throughput:        8420.09 txns/sec
Avg Latency:       46959.05 us
P50 Latency:       43351.00 us
P99 Latency:       100053.00 us
P99.9 Latency:     164448.00 us
```

### Same Continent (30ms ± 5ms)
```
Duration:          10054 ms
Total Txns:        48231
Committed:         48231
Aborted:           0 (0.00%)
Throughput:        4797.20 txns/sec
Avg Latency:       82771.05 us
P50 Latency:       80135.00 us
P99 Latency:       141282.00 us
P99.9 Latency:     176047.00 us
```

### Cross-Continent (80ms ± 10ms)
```
Duration:          10206 ms
Total Txns:        22303
Committed:         22303
Aborted:           0 (0.00%)
Throughput:        2185.28 txns/sec
Avg Latency:       181082.31 us
P50 Latency:       175449.00 us
P99 Latency:       297980.00 us
P99.9 Latency:     303055.00 us
```

### Cross-Region (150ms ± 20ms)
```
Duration:          10304 ms
Total Txns:        12262
Committed:         12262
Aborted:           0 (0.00%)
Throughput:        1190.02 txns/sec
Avg Latency:       330960.69 us
P50 Latency:       319846.00 us
P99 Latency:       466049.00 us
P99.9 Latency:     702192.00 us
```

---

## Comparison: 1 Thread vs 2 Threads

| Network | 1 Thread Throughput | 2 Thread Throughput | Scaling Factor |
|---------|---------------------|---------------------|----------------|
| Same Metro | 10,429 txns/sec | 8,420 txns/sec | 0.81x |
| Same Continent | 2,811 txns/sec | 4,797 txns/sec | 1.71x |
| Cross-Continent | 1,133 txns/sec | 2,185 txns/sec | 1.93x |
| Cross-Region | 614 txns/sec | 1,190 txns/sec | 1.94x |

**Analysis**: Low-latency scenarios show reduced throughput with 2 threads due to coordination overhead, while high-latency scenarios benefit significantly from concurrency as threads can overlap waiting periods.
