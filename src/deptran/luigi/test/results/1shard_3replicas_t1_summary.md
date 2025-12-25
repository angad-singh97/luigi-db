# Luigi 1-Shard 3-Replica Microbenchmark Results

## Thread Count: 1

### Test Configuration
- **Duration**: 10 seconds
- **Benchmark**: Micro (read-only transactions)
- **Topology**: 1 shard, 3 replicas

---

## Results Summary

| Network Condition | Latency | OWD | Headroom | Throughput | Avg Latency | P50 Latency | P99 Latency | P99.9 Latency |
|------------------|---------|-----|----------|------------|-------------|-------------|-------------|---------------|
| **Same Metro** | 2ms ± 0.5ms | 5ms | 2ms | 10,429 txns/sec | 18.4 ms | 17.4 ms | 39.9 ms | 128.5 ms |
| **Same Continent** | 30ms ± 5ms | 40ms | 10ms | 2,811 txns/sec | 70.3 ms | 67.0 ms | 108.1 ms | 246.8 ms |
| **Cross-Continent** | 80ms ± 10ms | 100ms | 20ms | 1,133 txns/sec | 174.2 ms | 168.4 ms | 254.5 ms | 323.4 ms |
| **Cross-Region** | 150ms ± 20ms | 180ms | 30ms | 614 txns/sec | 321.2 ms | 314.6 ms | 424.1 ms | 426.4 ms |

---

## Key Observations

1. **Throughput scales inversely with network latency**: As expected, throughput decreases significantly as network latency increases
   - Same Metro: ~10.4K txns/sec
   - Cross-Region: ~614 txns/sec (17x reduction)

2. **Average latency correlates with network conditions**: The average latency closely tracks the configured network latency plus protocol overhead
   - Same Metro: 18.4ms (vs 2ms network latency)
   - Cross-Region: 321.2ms (vs 150ms network latency)

3. **Tail latency behavior**: P99 and P99.9 latencies show reasonable bounds relative to average latency, indicating stable performance

4. **Zero aborts**: All tests completed with 0% abort rate, demonstrating system stability

---

## Detailed Results

### Same Metro (2ms ± 0.5ms)
```
Duration:          10023 ms
Total Txns:        104531
Committed:         104531
Aborted:           0 (0.00%)
Throughput:        10429.11 txns/sec
Avg Latency:       18420.67 us
P50 Latency:       17411.00 us
P99 Latency:       39929.00 us
P99.9 Latency:     128533.00 us
```

### Same Continent (30ms ± 5ms)
```
Duration:          10071 ms
Total Txns:        28309
Committed:         28309
Aborted:           0 (0.00%)
Throughput:        2810.94 txns/sec
Avg Latency:       70282.30 us
P50 Latency:       66965.00 us
P99 Latency:       108073.00 us
P99.9 Latency:     246759.00 us
```

### Cross-Continent (80ms ± 10ms)
```
Duration:          10163 ms
Total Txns:        11515
Committed:         11515
Aborted:           0 (0.00%)
Throughput:        1133.03 txns/sec
Avg Latency:       174184.36 us
P50 Latency:       168358.00 us
P99 Latency:       254529.00 us
P99.9 Latency:     323393.00 us
```

### Cross-Region (150ms ± 20ms)
```
Duration:          10282 ms
Total Txns:        6309
Committed:         6309
Aborted:           0 (0.00%)
Throughput:        613.60 txns/sec
Avg Latency:       321179.13 us
P50 Latency:       314609.00 us
P99 Latency:       424125.00 us
P99.9 Latency:     426422.00 us
```

---

## Next Steps

Phase 2 will test with higher thread counts (2, 4, and 8) to evaluate throughput scaling under concurrent load.
