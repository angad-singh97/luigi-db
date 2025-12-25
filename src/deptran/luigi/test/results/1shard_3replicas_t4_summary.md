# Luigi 1-Shard 3-Replica Microbenchmark Results

## Thread Count: 4

### Test Configuration
- **Duration**: 10 seconds
- **Benchmark**: Micro (read-only transactions)
- **Topology**: 1 shard, 3 replicas

---

## Results Summary

| Network Condition | Latency | OWD | Headroom | Throughput | Avg Latency | P50 Latency | P99 Latency | P99.9 Latency |
|------------------|---------|-----|----------|------------|-------------|-------------|-------------|---------------|
| **Same Metro** | 2ms ± 0.5ms | 5ms | 2ms | 6,884 txns/sec | 115.3 ms | 114.2 ms | 206.6 ms | 246.3 ms |
| **Same Continent** | 30ms ± 5ms | 40ms | 10ms | 5,467 txns/sec | 145.3 ms | 140.5 ms | 247.6 ms | 285.8 ms |
| **Cross-Continent** | 80ms ± 10ms | 100ms | 20ms | 3,290 txns/sec | 241.1 ms | 242.4 ms | 423.4 ms | 433.5 ms |
| **Cross-Region** | 150ms ± 20ms | 180ms | 30ms | 2,188 txns/sec | 363.5 ms | 355.4 ms | 492.6 ms | 735.0 ms |

---

## Key Observations

1. **Throughput comparison with 2 threads**:
   - Same Metro: 6,884 vs 8,420 txns/sec (18% reduction)
   - Same Continent: 5,467 vs 4,797 txns/sec (14% increase)
   - Cross-Continent: 3,290 vs 2,185 txns/sec (51% increase)
   - Cross-Region: 2,188 vs 1,190 txns/sec (84% increase)

2. **Concurrency scaling**: Higher thread counts continue to benefit high-latency scenarios more than low-latency ones

3. **Latency degradation**: Average latency increases with thread count due to higher concurrent load and contention

4. **Zero aborts**: All tests completed with 0% abort rate

---

## Detailed Results

### Same Metro (2ms ± 0.5ms)
```
Duration:          10084 ms
Total Txns:        69423
Committed:         69423
Aborted:           0 (0.00%)
Throughput:        6884.47 txns/sec
Avg Latency:       115343.58 us
P50 Latency:       114158.00 us
P99 Latency:       206605.00 us
P99.9 Latency:     246252.00 us
```

### Same Continent (30ms ± 5ms)
```
Duration:          10141 ms
Total Txns:        55436
Committed:         55436
Aborted:           0 (0.00%)
Throughput:        5466.52 txns/sec
Avg Latency:       145261.96 us
P50 Latency:       140507.00 us
P99 Latency:       247648.00 us
P99.9 Latency:     285807.00 us
```

### Cross-Continent (80ms ± 10ms)
```
Duration:          10236 ms
Total Txns:        33676
Committed:         33676
Aborted:           0 (0.00%)
Throughput:        3289.96 txns/sec
Avg Latency:       241143.85 us
P50 Latency:       242379.00 us
P99 Latency:       423396.00 us
P99.9 Latency:     433479.00 us
```

### Cross-Region (150ms ± 20ms)
```
Duration:          10280 ms
Total Txns:        22490
Committed:         22490
Aborted:           0 (0.00%)
Throughput:        2187.74 txns/sec
Avg Latency:       363475.10 us
P50 Latency:       355415.00 us
P99 Latency:       492634.00 us
P99.9 Latency:     735046.00 us
```

---

## Comparison: Thread Count Scaling

| Network | 1T | 2T | 4T | 2T vs 1T | 4T vs 2T |
|---------|----|----|----|---------|---------| 
| Same Metro | 10,429 | 8,420 | 6,884 | 0.81x | 0.82x |
| Same Continent | 2,811 | 4,797 | 5,467 | 1.71x | 1.14x |
| Cross-Continent | 1,133 | 2,185 | 3,290 | 1.93x | 1.51x |
| Cross-Region | 614 | 1,190 | 2,188 | 1.94x | 1.84x |

**Analysis**: 
- Low-latency (Same Metro) shows diminishing returns with more threads due to coordination overhead
- High-latency scenarios continue to scale well with thread count, nearly doubling from 2T to 4T for Cross-Region
- The benefit of concurrency is most pronounced when network latency dominates transaction time
