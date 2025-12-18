# Luigi Configuration Files

This directory contains YAML configuration files for Luigi benchmarks in **Mako-compatible format**.

## Configuration Format

Luigi uses Mako's `transport::Configuration` parser, so configs must follow the **site → process → host** hierarchy:

```yaml
site:
  server:  # List of shards (each shard can have multiple replicas)
    - ["s0:31850"]           # Shard 0, no replication
    - ["s1:31851", ...]      # Shard 1 with replicas
  client:
    - ["c0"]                 # Client processes

process:  # Map site names to process names
  s0: localhost
  c0: localhost

host:  # Map process names to IP addresses
  localhost: 127.0.0.1

# Luigi-specific section
luigi:
  client_vm_index: 0
  num_workers_per_vm: 4
  headroom_ms: 2
  ping_interval_ms: 100
  initial_owd_ms: 1
```

## Configuration Files

### No Replication
- **`local-1shard.yml`**: 1 shard, 4 threads, 4 warehouses
- **`local-2shard.yml`**: 2 shards, 6 threads, 6 warehouses

### With Replication (3 replicas per shard)
- **`local-1shard-3replicas.yml`**: 1 shard × 3 replicas, 4 threads
- **`local-2shard-3replicas.yml`**: 2 shards × 3 replicas each, 6 threads

## Usage

### Single Shard
```bash
# Shard 0
./build/luigi_bench --shard-config src/deptran/luigi/config/local-1shard.yml \
                    --shard-index 0 --num-threads 4 --benchmark tpcc
```

### Two Shards
```bash
# Terminal 1: Shard 0
./build/luigi_bench --shard-config src/deptran/luigi/config/local-2shard.yml \
                    --shard-index 0 --num-threads 6 --benchmark tpcc &

# Terminal 2: Shard 1
./build/luigi_bench --shard-config src/deptran/luigi/config/local-2shard.yml \
                    --shard-index 1 --num-threads 6 --benchmark tpcc &
```

**Note**: Same config file used for all shards, `--shard-index` determines which shard each process runs.

## Luigi Parameters

- `client_vm_index`: Index of this client VM (0, 1, 2, ...)
- `num_workers_per_vm`: Worker threads per VM
- `headroom_ms`: OWD headroom in milliseconds
- `ping_interval_ms`: OWD ping interval
- `initial_owd_ms`: Initial OWD estimate

## Worker ID Calculation
```
worker_id = client_vm_index * num_workers_per_vm + thread_id
```

**Example** (2 VMs, 6 threads each):
- VM 0: worker IDs 0-5
- VM 1: worker IDs 6-11

## Matching Mako Test Scenarios

These configs match Mako's CI test scenarios:
- `local-1shard.yml` → Similar to `1c1s1p.yml`
- `local-2shard.yml` → Similar to `test_2shard_no_replication.sh`
- `local-1shard-3replicas.yml` → Similar to `1c1s3r1p.yml`
- `local-2shard-3replicas.yml` → Similar to `test_2shard_replication.sh`
