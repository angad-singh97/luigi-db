# Unified Makefile - Builds both Mako Paxos and Jetpack Raft
# Usage:
#   make              - Build production (Paxos + Raft)
#   make raft-test    - Build with Raft testing coroutines enabled
#   make clean        - Clean build artifacts

# Variables
BUILD_DIR = build
CMAKE_OPTIONS ?= -DPAXOS_LIB_ENABLED=1

.PHONY: all configure build clean rebuild run raft-test help

all: build

configure:
	cmake -S . -B $(BUILD_DIR) $(CMAKE_OPTIONS)

build: configure
	cmake --build $(BUILD_DIR) --parallel

# Build with Raft testing coroutines enabled
raft-test: CMAKE_OPTIONS += -DRAFT_TEST=ON
raft-test: rebuild

clean:
	rm -rf $(BUILD_DIR)
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

rebuild: clean all

run: build
	./$(BUILD_DIR)/dbtest
	./$(BUILD_DIR)/simpleTransction
	./$(BUILD_DIR)/simpleTransctionRep
	./$(BUILD_DIR)/simplePaxos

help:
	@echo "Unified Build System - Mako Paxos + Jetpack Raft"
	@echo ""
	@echo "Usage:"
	@echo "  make              - Build production (both Paxos and Raft)"
	@echo "  make raft-test    - Build with Raft testing coroutines"
	@echo "  make clean        - Clean all build artifacts"
	@echo "  make rebuild      - Clean and rebuild"
	@echo ""
	@echo "Testing:"
	@echo "  ./ci/ci.sh simplePaxos                           - Test Paxos"
	@echo "  ./build/deptran_server -f config/3c1s3r3p.yml    - Run production Raft"
	@echo "  ./build/deptran_server -f config/raft_lab_test.yml - Run Raft lab tests (requires make raft-test)"
