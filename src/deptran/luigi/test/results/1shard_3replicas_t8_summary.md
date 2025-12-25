# Luigi 1-Shard 3-Replica Microbenchmark Results

## Thread Count: 8

### Test Configuration
- **Duration**: 10 seconds
- **Benchmark**: Micro (read-only transactions)
- **Topology**: 1 shard, 3 replicas

---

## Results Summary

| Network Condition | Latency | OWD | Headroom | Throughput | Avg Latency | P50 Latency | P99 Latency | P99.9 Latency |
|------------------|---------|-----|----------|------------|-------------|-------------|-------------|---------------|
| **Same Metro** | 2ms ± 0.5ms | 5ms | 2ms | 5,235 txns/sec | 303.5 ms | 298.5 ms | 807.5 ms | 831.4 ms |
| **Same Continent** | 30ms ± 5ms | 40ms | 10ms | 4,701 txns/sec | 336.5 ms | 333.4 ms | 529.7 ms | 546.8 ms |
| **Cross-Continent** | 80ms ± 10ms | 100ms | 20ms | 4,521 txns/sec | 347.1 ms | 338.1 ms | 543.3 ms | 588.9 ms |
| **Cross-Region** | 150ms ± 20ms | 180ms | 30ms | 3,261 txns/sec | 478.7 ms | 436.4 ms | 940.5 ms | 1020.8 ms |

---

## Key Observations

1. **Throughput comparison with 4 threads**:
   - Same Metro: 5,235 vs 6,884 txns/sec (24% reduction)
   - Same Continent: 4,701 vs 5,467 txns/sec (14% reduction)
   - Cross-Continent: 4,521 vs 3,290 txns/sec (37% increase)
   - Cross-Region: 3,261 vs 2,188 txns/sec (49% increase)

2. **Diminishing returns for low latency**: Same Metro and Same Continent show reduced throughput with 8 threads compared to 4 threads

3. **Continued scaling for high latency**: Cross-Continent and Cross-Region continue to benefit from higher thread counts

4. **Latency degradation**: Significant increase in average latency with 8 threads, especially for low-latency scenarios

5. **Zero aborts**: All tests completed with 0% abort rate

---

## Detailed Results

### Same Metro (2ms ± 0.5ms)
```
Duration:          10123 ms
Total Txns:        52995
Committed:         52995
Aborted:           0 (0.00%)
Throughput:        5235.11 txns/sec
Avg Latency:       303541.92 us
P50 Latency:       298514.00 us
P99 Latency:       807484.00 us
P99.9 Latency:     831373.00 us
```

### Same Continent (30ms ± 5ms)
```
Duration:          10152 ms
Total Txns:        47727
Committed:         47727
Aborted:           0 (0.00%)
Throughput:        4701.24 txns/sec
Avg Latency:       336517.84 us
P50 Latency:       333378.00 us
P99 Latency:       529716.00 us
P99.9 Latency:     546825.00 us
```

### Cross-Continent (80ms ± 10ms)
```
Duration:          10264 ms
Total Txns:        46404
Committed:         46404
Aborted:           0 (0.00%)
Throughput:        4521.04 txns/sec
Avg Latency:       347085.36 us
P50 Latency:       338072.00 us
P99 Latency:       543344.00 us
P99.9 Latency:     588941.00 us
```

### Cross-Region (150ms ± 20ms)
```
Duration:          10445 ms
Total Txns:        34060
Committed:         34060
Aborted:           0 (0.00%))
Throughput:        3260.89 txns/sec
Avg Latency:       478746.35 us
P50 Latency:       436359.00 us
P99 Latency:       940451.00 us
P99.9 Latency:     1020762.00 us
```

---

## Comparison: Thread Count Scaling

| Network | 1T | 2T | 4T | 8T | Peak Throughput |
|---------|----|----|----|----|-----------------|
| Same Metro | 10,429 | 8,420 | 6,884 | 5,235 | **1T: 10,429** |
| Same Continent | 2,811 | 4,797 | 5,467 | 4,701 | **4T: 5,467** |
| Cross-Continent | 1,133 | 2,185 | 3,290 | 4,521 | **8T: 4,521** |
| Cross-Region | 614 | 1,190 | 2,188 | 3,261 | **8T: 3,261** |

**Analysis**: 
- **Low-latency scenarios (Same Metro)**: Peak performance at 1 thread, diminishing returns with concurrency
- **Medium-latency (Same Continent)**: Peak at 4 threads, showing optimal balance
- **High-latency scenarios (Cross-Continent, Cross-Region)**: Continue scaling through 8 threads, with Cross-Region showing 5.3x improvement from 1T to 8T
- **Optimal thread count varies by network latency**: Lower latency benefits from fewer threads, higher latency benefits from more threads
