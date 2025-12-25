# Luigi Test Suite

Comprehensive testing infrastructure for Luigi distributed transaction system.

## Directory Structure

```
test/
├── scripts/           # Test automation scripts
├── configs/           # Test configuration files
└── results/           # Test results (generated)
```

## Quick Start

### Run All Tests

```bash
cd src/deptran/luigi/test
./scripts/run_all_tests.sh
```

This will execute all experiments and generate a comprehensive report.

### Run Individual Tests

```bash
# Worker count optimization
./scripts/run_worker_count_sweep.sh

# Microbenchmark suite
./scripts/run_microbench_suite.sh

# Network latency sweep
./scripts/run_network_latency_sweep.sh

# Cross-shard transaction sweep
./scripts/run_cross_shard_sweep.sh

# TPC-C benchmark suite
./scripts/run_tpcc_suite.sh
```

## WAN Simulation

To simulate WAN latency (100ms + 20ms jitter):

```bash
# Enable WAN simulation
sudo ./scripts/setup_wan_simulation.sh

# Run tests...

# Disable WAN simulation
sudo ./scripts/cleanup_wan_simulation.sh
```

## Test Configurations

- `1shard-norep.yml` - Single shard, no replication
- `2shard-norep.yml` - 2 shards, no replication
- `1shard-rep.yml` - Single shard, 3x replication
- `2shard-rep.yml` - 2 shards, 3x replication

## Results

Results are saved in `results/` with timestamped subdirectories for each run.

Each test generates:
- CSV files with raw data
- PNG graphs (if gnuplot available)
- Markdown reports

## Requirements

- Luigi built in `../../../build/`
- `gnuplot` (optional, for graphs)
- `sudo` access (for WAN simulation)

## See Also

- [TEST_PLAN.md](../TEST_PLAN.md) - Detailed test plan and expected results
