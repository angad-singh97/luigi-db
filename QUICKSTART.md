# Quick Start Guide

## Simple Build Commands

The entire codebase (Mako Paxos + Jetpack Raft) builds with a single command:

### 1. Build Production (Default)

```bash
make
```

This builds **both** Mako Paxos and Jetpack Raft in production mode.

### 2. Build with Raft Testing Coroutines

```bash
make raft-test
```

This builds with the `RAFT_TEST_CORO` flag enabled for Raft lab tests.

### 3. Clean Build

```bash
make clean
make
```

## Testing

### Test Mako Paxos
```bash
./ci/ci.sh simplePaxos
```

### Run Production Raft
```bash
./build/deptran_server -f config/3c1s3r3p.yml
```

### Run Raft Lab Tests
```bash
# First build with raft-test flag
make raft-test

# Then run lab tests
./build/deptran_server -f config/raft_lab_test.yml
```

## What Gets Built?

One unified binary containing:
- ✅ Mako Paxos (bulk operations, failover, etc.)
- ✅ Jetpack Raft (consensus, replication)
- ✅ All RPC services
- ✅ Shared infrastructure (eRPC, coroutines, etc.)

The protocol used is determined by the **config file**, not the build.

## Help

```bash
make help
```

## Full Workflow Example

```bash
# 1. Build everything (first time: 10-30 minutes)
make

# 2. Test Paxos
./ci/ci.sh simplePaxos

# 3. Test production Raft
./build/deptran_server -f config/3c1s3r3p.yml

# 4. For Raft lab testing, rebuild with flag
make raft-test
./build/deptran_server -f config/raft_lab_test.yml
```

## Advanced: Direct CMake

If you need more control:

```bash
mkdir -p build && cd build
cmake .. -DPAXOS_LIB_ENABLED=1 -DRAFT_TEST=ON -DSHARDS=4
make -j32
```

See `BUILD_INSTRUCTIONS.md` for all CMake options.
