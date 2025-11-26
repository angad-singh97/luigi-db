# Unified Makefile - Builds both Mako Paxos and Jetpack Raft
# Usage:
#   make              - Build production (Paxos + Raft)
#   make raft-test    - Build with Raft testing coroutines enabled
#   make clean        - Clean build artifacts

# Variables
BUILD_DIR = build

RAFT_TEST_FLAG ?= OFF

PARALLEL_JOBS := $(if $(filter -j%,$(MAKEFLAGS)),$(subst -j,,$(filter -j%,$(MAKEFLAGS))),4)

.PHONY: all configure build clean rebuild run raft-test help test test-verbose test-parallel

all: build

configure:
	cmake -S . -B $(BUILD_DIR) -DRAFT_TEST=$(RAFT_TEST_FLAG)

build: configure
	@echo "Building with $(PARALLEL_JOBS) parallel jobs..."
	cmake --build $(BUILD_DIR) --parallel $(PARALLEL_JOBS)

# Build with Raft testing coroutines enabled without nuking existing build artifacts
raft-test:
	cmake -S . -B $(BUILD_DIR) -DRAFT_TEST=ON
	@echo "Building Raft test binaries with $(PARALLEL_JOBS) parallel jobs..."
	cmake --build $(BUILD_DIR) --parallel $(PARALLEL_JOBS)

# Build Mako with the Raft helper enabled
mako-raft:
	cmake -S . -B $(BUILD_DIR) -DMAKO_USE_RAFT=ON
	@echo "Building Mako with Raft helper using $(PARALLEL_JOBS) parallel jobs..."
	cmake --build $(BUILD_DIR) --parallel $(PARALLEL_JOBS)

clean:
	rm -rf $(BUILD_DIR) 2>/dev/null || true
	# Remove all test files
	rm -rf /tmp/test_*
	# Remove all disk db
	- rm -rf /tmp/rocksdb_*
	rm -rf /tmp/callback_demo_db*
	# rm -rf /tmp/mako_rocksdb*
	# Clean out-perf.masstree
	rm -rf ./out-perf.masstree/*
	# Clean mako out-perf.masstree
	rm -rf ./src/mako/out-perf.masstree/*
	# Clean Masstree configuration
	@echo "Cleaning Masstree configuration..."
	@cd src/mako/masstree && make distclean 2>/dev/null || true
	@rm -f src/mako/masstree/config.h src/mako/masstree/config.h.in
	@rm -f src/mako/masstree/configure src/mako/masstree/config.status
	@rm -f src/mako/masstree/config.log src/mako/masstree/GNUmakefile
	@rm -f src/mako/masstree/autom4te.cache -rf
	# Clean LZ4 library
	@echo "Cleaning LZ4 library..."
	@cd third-party/lz4 && make clean 2>/dev/null || true
	@rm -f third-party/lz4/liblz4.so third-party/lz4/*.o
	# Clean Rust library
	@echo "Cleaning Rust library..."
	@cd rust-lib && cargo clean 2>/dev/null || true
	# Clean rusty-cpp
	@rm -rf third-party/rusty-cpp/target || true
	# rebuild rpc
	bin/rpcgen --cpp --python src/deptran/rcc_rpc.rpc

rebuild: clean all

run: build
	./$(BUILD_DIR)/dbtest
	./$(BUILD_DIR)/simpleTransction
	./$(BUILD_DIR)/simpleTransctionRep
	./$(BUILD_DIR)/simplePaxos

# Run tests using ctest
test: build
	@echo "Running tests..."
	@cd $(BUILD_DIR) && ctest --output-on-failure

# Run tests with verbose output
test-verbose: build
	@echo "Running tests with verbose output..."
	@cd $(BUILD_DIR) && ctest --verbose --output-on-failure

# Run tests in parallel
test-parallel: build
	@echo "Running tests in parallel..."
	@cd $(BUILD_DIR) && ctest -j$(if $(filter -j%,$(MAKEFLAGS)),$(subst -j,,$(filter -j%,$(MAKEFLAGS))),4) --output-on-failure

help:
	@echo "Unified Build System - Mako Paxos + Jetpack Raft"
	@echo ""
	@echo "Usage:"
	@echo "  make              - Build production (both Paxos and Raft)"
	@echo "  make raft-test    - Build with Raft testing coroutines"
	@echo "  make clean        - Clean all build artifacts"
	@echo "  make rebuild      - Clean and rebuild"
	@echo "  make test         - Run ctest test suite"
	@echo "  make test-verbose - Run tests with verbose output"
	@echo "  make test-parallel- Run tests in parallel"
	@echo ""
	@echo "Testing:"
	@echo "  ./ci/ci.sh simplePaxos                           - Test Paxos"
	@echo "  ./build/deptran_server -f config/3c1s3r3p.yml    - Run production Raft"
	@echo "  ./build/deptran_server -f config/raft_lab_test.yml - Run Raft lab tests (requires make raft-test)"
