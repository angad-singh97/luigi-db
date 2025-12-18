# Luigi Configuration Files

This directory contains YAML configuration files for Luigi benchmarks.

## Configuration Files

### Single-VM Testing
- **`single_vm_test.yml`**: Local testing with one client VM
  - 4 worker threads (worker IDs: 0-3)
  - 2 shards on localhost
  - 30 second duration
  - Replication disabled

### Multi-VM Testing
- **`multi_vm_client0.yml`**: Configuration for client VM 0
  - 8 worker threads (worker IDs: 0-7)
  - 3 shards on cluster
  - 60 second duration
  - Replication enabled

- **`multi_vm_client1.yml`**: Configuration for client VM 1
  - 8 worker threads (worker IDs: 8-15)
  - 3 shards on cluster
  - 60 second duration
  - Replication enabled

## Configuration Parameters

### Luigi Section
- `client_vm_index`: Index of this client VM (0, 1, 2, ...)
- `num_workers_per_vm`: Number of worker threads per VM
- `headroom_ms`: OWD headroom in milliseconds
- `ping_interval_ms`: OWD ping interval
- `initial_owd_ms`: Initial OWD estimate

### Worker ID Calculation
```
worker_id_base = client_vm_index * num_workers_per_vm
worker_id_range = [worker_id_base, worker_id_base + num_workers_per_vm - 1]
```

**Example**:
- VM 0: `client_vm_index=0`, `num_workers_per_vm=8` → worker IDs: 0-7
- VM 1: `client_vm_index=1`, `num_workers_per_vm=8` → worker IDs: 8-15
- VM 2: `client_vm_index=2`, `num_workers_per_vm=8` → worker IDs: 16-23

## Usage

### Single-VM Testing
```bash
cd /root/cse532/mako
./build/luigi_benchmark_client --config src/deptran/luigi/config/single_vm_test.yml
```

### Multi-VM Testing
On client VM 0:
```bash
./build/luigi_benchmark_client --config src/deptran/luigi/config/multi_vm_client0.yml
```

On client VM 1:
```bash
./build/luigi_benchmark_client --config src/deptran/luigi/config/multi_vm_client1.yml
```

## Notes
- Ensure all VMs can reach the shard servers
- Worker IDs must not overlap between VMs
- OWD parameters may need tuning based on network latency
