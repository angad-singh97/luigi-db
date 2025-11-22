# Masstree RustyCpp Migration

This folder tracks the plan and progress for migrating the Masstree storage engine and its surrounding wrappers to RustyCpp borrow checking. Masstree is deeply integrated into `src/mako/`, so the migration needs coordination with the transaction layer, RCU subsystem, and tooling.

## Files
- `masstree-rustycpp-migration-plan.md` – detailed roadmap with phases, owners, risks, and success metrics.
- (future) progress checklists, per-module notes, or design discussions can live next to the plan as needed.

## Test & Benchmark Entrypoints
- `test_masstree` – gtest-based regression suite that exercises inserts, lookups, range scans, and removals. It is built automatically and runs whenever `make test`/`ctest` or `make run_tests` executes.
- `masstree_perf` – on-demand micro-benchmark. Run it manually (`./build/masstree_perf --output perf.json --baseline prev.json --scan-window 512`) to capture JSON summaries for multiple workloads:
  - Sequential/random inserts
  - Random lookups
  - Mixed read/write traffic
  - Range scans (configurable window via `--scan-window`)
  - Sequential removes
  Each scenario reports ops/sec and operation counts, and the tool can diff runs via `--baseline`.

## Related Documents
- `doc/rrr-rustycpp-migration-plan.md` – reference roadmap for the RRR/RPC subsystem.
- `doc/RRR_SAFETY_ROADMAP.md` – high-level safety goals that inspired the Masstree effort.
- `src/mako/masstree_btree.h`, `src/mako/base_txn_btree.h`, and `src/mako/rcu.h` – primary code that will be annotated.

Use this directory as the single source of truth for Masstree migration status so that future contributors can quickly understand what remains.
