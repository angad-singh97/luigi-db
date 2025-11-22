# Masstree RustyCpp Migration Plan


**Objectives**
- Earn RustyCpp coverage for Masstree sources (including `masstree_btree.h`, `base_txn_btree.h`, `txn_btree.*`, and the vendored `src/mako/masstree/*`).
- Reduce raw pointer usage in safe code paths; confine unavoidable cases to well-documented `@unsafe` blocks.
- Preserve performance targets (sub-microsecond point lookups, multi-million ops/sec per core) while enforcing ownership rules.

## File Catalog & Scope
The following files make up the Masstree surface that must receive `@safe`/`@unsafe` annotations during the migration. Grouping the scope avoids surprises during later phases and mirrors the breadth of the recent RRR safety work (latest commits touched `marshal`, `pollthread`, futures, etc.).

### Integration Wrappers & Entry Points (`src/mako/`)
- `masstree_btree.h`
- `base_txn_btree.h`
- `txn_btree.h`, `txn_btree.cc`
- `typed_txn_btree.h`
- `txn_proto2_impl.h`, `txn_proto2_impl.cc` (Masstree-backed transactional logic)
- `txn.h`, `txn.cc` (interfaces invoked directly by Masstree-backed code)
- `tuple.h`, `tuple.cc`, `tuple_btree.cc` (tuple storage for Masstree pages)
- `ownership_checker.h`
- `rcu.h`, `rcu.cc`
- `prefetch.h`, `amd64.h`, `macros.h` (utility headers Masstree relies on)

### Vendored Masstree Sources (`src/mako/masstree/`)
These files come directly from the upstream Masstree drop and must be annotated in-place so downstream wrappers can remain `@safe`. The list intentionally includes configuration helpers and benchmarks because they exercise the same raw structures.

```
src/mako/masstree/AUTHORS
src/mako/masstree/GNUmakefile.in
src/mako/masstree/LICENSE
src/mako/masstree/README.md
src/mako/masstree/_masstree_config.d
src/mako/masstree/bootstrap.sh
src/mako/masstree/btree_leaflink.hh
src/mako/masstree/checkpoint.cc
src/mako/masstree/checkpoint.hh
src/mako/masstree/circular_int.hh
src/mako/masstree/clp.c
src/mako/masstree/clp.h
src/mako/masstree/compiler.cc
src/mako/masstree/compiler.hh
src/mako/masstree/configure.ac
src/mako/masstree/doc/.gitignore
src/mako/masstree/doc/GNUmakefile
src/mako/masstree/doc/elements.mp
src/mako/masstree/doc/elemfig.sty
src/mako/masstree/doc/examples.mp
src/mako/masstree/doc/insert1.mp
src/mako/masstree/doc/masstree.mp
src/mako/masstree/doc/patches.mp
src/mako/masstree/doc/remove1.mp
src/mako/masstree/doc/remove2.mp
src/mako/masstree/doc/spec.tex
src/mako/masstree/file.cc
src/mako/masstree/file.hh
src/mako/masstree/hashcode.hh
src/mako/masstree/json.cc
src/mako/masstree/json.hh
src/mako/masstree/jsontest.cc
src/mako/masstree/kpermuter.hh
src/mako/masstree/ksearch.hh
src/mako/masstree/kvio.cc
src/mako/masstree/kvio.hh
src/mako/masstree/kvproto.hh
src/mako/masstree/kvrandom.cc
src/mako/masstree/kvrandom.hh
src/mako/masstree/kvrow.hh
src/mako/masstree/kvstats.hh
src/mako/masstree/kvtest.hh
src/mako/masstree/kvthread.cc
src/mako/masstree/kvthread.hh
src/mako/masstree/masstree.hh
src/mako/masstree/masstree_get.hh
src/mako/masstree/masstree_insert.hh
src/mako/masstree/masstree_key.hh
src/mako/masstree/masstree_print.hh
src/mako/masstree/masstree_remove.hh
src/mako/masstree/masstree_scan.hh
src/mako/masstree/masstree_split.hh
src/mako/masstree/masstree_struct.hh
src/mako/masstree/masstree_tcursor.hh
src/mako/masstree/memdebug.cc
src/mako/masstree/memdebug.hh
src/mako/masstree/misc.cc
src/mako/masstree/misc.hh
src/mako/masstree/msgpack.cc
src/mako/masstree/msgpack.hh
src/mako/masstree/msgpacktest.cc
src/mako/masstree/mtclient.cc
src/mako/masstree/mtclient.hh
src/mako/masstree/mtcounters.hh
src/mako/masstree/mtd.cc
src/mako/masstree/mttest.cc
src/mako/masstree/nodeversion.hh
src/mako/masstree/perfstat.cc
src/mako/masstree/perfstat.hh
src/mako/masstree/query_masstree.cc
src/mako/masstree/query_masstree.hh
src/mako/masstree/scantest.cc
src/mako/masstree/small_vector.hh
src/mako/masstree/str.cc
src/mako/masstree/str.hh
src/mako/masstree/straccum.cc
src/mako/masstree/straccum.hh
src/mako/masstree/string.cc
src/mako/masstree/string.hh
src/mako/masstree/string_base.hh
src/mako/masstree/string_slice.cc
src/mako/masstree/string_slice.hh
src/mako/masstree/stringbag.hh
src/mako/masstree/test_atomics.cc
src/mako/masstree/test_string.cc
src/mako/masstree/testrunner.cc
src/mako/masstree/testrunner.hh
src/mako/masstree/timestamp.hh
src/mako/masstree/value_array.cc
src/mako/masstree/value_array.hh
src/mako/masstree/value_bag.hh
src/mako/masstree/value_string.cc
src/mako/masstree/value_string.hh
src/mako/masstree/value_versioned_array.cc
src/mako/masstree/value_versioned_array.hh
```

### Benchmarks & Harnesses Outside the Vendored Tree
- `src/mako/benchmarks/sto/masstree-beta/**` (STO microbench harness; mirrors upstream files and must stay consistent)
- `src/mako/benchmarks/tpcc.cc` and `benchmarks/rpc_setup.cc` (exercise Masstree transactions directly)
- `src/mako/benchmarks/encstress.cc`, `benchmarks/bid.cc`, `benchmarks/queue.cc` (each manipulates Masstree-backed structures)
- Tooling scripts under `src/mako/masstree/` (`mtd.cc`, `mtclient.cc`, `mttest.cc`, `scantest.cc`) noted above but called out here because they will be recompiled with borrow checking once annotations land.

Keep this catalog synced as work proceeds—add/remove entries here before touching code so the migration scope stays transparent.

## Phase 0 – Current State Snapshot (Week 0)
- [ ] Capture baseline borrow-check results by enabling the checker for a single Masstree translation unit (`txn_btree.cc`).
- [ ] Record perf & memory profiles from `benchmarks/sto/masstree-beta/testrunner.cc` and `benchmarks/tpcc.cc` for comparison after migration.
- [ ] Document existing RCU invariants (`src/mako/rcu.h`) and timestamp semantics used by `simple_threadinfo`.

Artifacts: short summary appended to this doc + pointers to logs.

---

## Phase 1 – Assessment & Infrastructure (Week 1)
### 1.1 Module Inventory & Ownership
| Area | Files / Directories | Notes |
|------|--------------------|-------|
| Core tree implementation | `src/mako/masstree/*.hh`, `masstree/*.cc` | Header-heavy; templates instantiate everywhere. |
| Wrappers & API surface | `src/mako/masstree_btree.h`, `base_txn_btree.h`, `txn_btree.*` | Provide concurrency control & tuple integration. |
| Supporting subsystems | `src/mako/rcu.*`, `ownership_checker.h`, `prefetch.h`, `amd64.h` | Provide allocators, fences, lock tracking. |
| Tooling/tests | `src/mako/masstree/testrunner.*`, `benchmarks/sto/masstree-beta/*` | Need borrow-check builds. |

Tasks
- [ ] Map every translation unit that includes `masstree_btree.h` (via `rg -l "masstree_btree"`).
- [ ] Identify all functions that manipulate `node_base`, `leaf`, or `internode` pointers directly.
- [ ] Catalog unsafe operations (memcpy/memmove, manual parent pointers, RCU callbacks).

### 1.2 Borrow Checking Enablement
- [ ] Update CMake to let `enable_borrow_checking()` cover the `mako` target (currently limited to dbtest).
- [ ] Create a dedicated borrow-check target (e.g., `masstree_safety`) that compiles `txn_btree.cc` + a stub main with `#pragma safe`.
- [ ] Add CI job / Git hook to run the checker whenever masstree files change.

### 1.3 Safety Guidelines
- [ ] Define `@safe`/`@unsafe` usage patterns specific to Masstree (RCU, timestamping, pointer tagging).
- [ ] Establish canonical wrappers for frequently-used raw structures (e.g., `NodeHandle { node_base*, lifetime }`).
- [ ] Extend the RustyCpp annotations guide with a Masstree section (lifetime regions for nodes, tuple buffers, phantom epochs).

Exit Criteria
- Borrow checker runs over at least one Masstree TU in CI.
- Annotated guideline doc linked from this plan.

---

## Phase 2 – Core Data Structures (Weeks 2–3)
Focus on the header-only types that model the B+tree structure.

| Component | Key Files | Risks / Work | Unsafe Expectations |
|-----------|-----------|--------------|---------------------|
| `node_base`, `leaf`, `internode` | `src/mako/masstree/masstree_struct.hh` | Raw parent/child pointers, manual allocations (`pool_allocate`), unchecked memcpy; need RAII wrappers and invariant docs. | Minimal `@unsafe` loops encapsulated in helper functions for copy/move of node arrays. |
| Keys & cursors | `masstree_key.hh`, `masstree_tcursor.hh`, `masstree_scan.hh` | Lifetime of `key<ikey>` views; pointer arithmetic for layered tries; ensure returned slices do not outlive nodes. | Allowed for pointer casting between layers with invariants. |
| Value holders | `leafvalue`, `stringbag`, `mtcounters` | Manage embedded storage + string arenas; convert to safe buffer views. | Possibly required when overlaying union storage. |

Deliverables
- Typedefs for `NodeId`, `NodeVersion`, etc. with constructors enforcing invariants.
- Functions like `internode::shift_*` rewritten to call `copy_range_safe(dst, src, count)` where the unsafe block is centralized and documented.
- Borrow-check clean `basic_table` API: `get`, `scan`, `modify` annotated @safe.

Testing
- Run `src/mako/masstree/testrunner` with borrow-check build flags.
- Compare tree dumps (via `print()`) before/after to ensure deterministic structure.

---

## Phase 3 – Threadinfo, RCU & Memory Management (Weeks 3–4)
| Area | Files | Tasks |
|------|-------|-------|
| `simple_threadinfo` | `src/mako/masstree_btree.h` | Define safe trait exposing timestamps, counters, and allocation methods; wrap raw pointers returned from `rcu`. |
| RCU subsystem | `src/mako/rcu.h`, `src/mako/rcu.cc` | Add safe wrapper types (`RcuBox`, `DeferredFreeToken`); limit direct `void*` arithmetic to `@unsafe` helpers with safety comments. |
| Ownership checker | `src/mako/ownership_checker.h` | Replace `std::vector<const T*>` with Rusty containers; ensure tracked nodes carry borrow tokens instead of raw pointers. |

Key Actions
- Document the epoch / phantom timestamp contract (ts_ advances monotonically, ensures odd/even semantics) and enforce via helper functions checked by RustyCpp.
- Introduce RAII types for `pool_allocate`/`pool_deallocate` so nodes are always freed through typed wrappers.
- Make `rcu_register` accept safe functors whose captured data is `Send + 'static` equivalent; only the trampoline stays `@unsafe`.

---

## Phase 4 – API Surface & Transaction Integration (Weeks 4–5)
| Layer | Files | Changes |
|-------|-------|---------|
| `mbtree` API | `src/mako/masstree_btree.h` | Replace `uint8_t*` value buffers with `rusty::Vec<uint8_t>` / `rusty::Box<T>` handles; make cursors yield typed borrows; add safe iterators. |
| Transaction wrappers | `src/mako/base_txn_btree.h`, `src/mako/txn_btree.*`, `src/mako/typed_txn_btree.h` | Ensure search/insert/remove operate on safe handles; callbacks use `rusty::Arc`/`Weak` for captures; annotate tuple writer/reader lifetimes. |
| Benchmarks & tooling | `src/mako/benchmarks/*masstree*`, `src/mako/masstree/*.cc` | Update to new safe APIs; mark legacy benchmarking code `@unsafe` only where necessary. |

Additional Work
- Introduce helper macros/functions to start/end “node lock regions” safely, replacing manual `ownership_checker` interaction.
- Provide compatibility shims for external code (e.g., autop-runner scripts) using old APIs; mark as deprecated.

---

## Phase 5 – Integration, Validation & Documentation (Weeks 5–6)
- [ ] Extend borrow checking to every binary that links Masstree (mako server, benchmarks, tools).
- [ ] Run ASan + Valgrind on borrow-check builds to catch regressions.
- [ ] Update `doc/architecture.md` and `doc/concepts.md` to mention the Masstree safety posture and how to write @safe code.
- [ ] Produce migration notes for downstream consumers (e.g., any team embedding Masstree outside of Mako).

Success Criteria
- 100% of Masstree-facing source files compile with RustyCpp borrow checking enabled.
- No uncategorized `@unsafe` code remains; all unsafe blocks include “SAFETY:” comments explaining invariants and tests.
- Performance regressions within ±5% of baseline for TPCC and STO/masstree benchmarks.

---

## Implementation Guidelines

### Safe Conversion Patterns
1. **Parent/child pointer access** – wrap raw `node_base*` in `NodeHandle` that records the owning `threadinfo` and version; only `NodeHandle::raw()` is `@unsafe`.
2. **Node splits & merges** – centralize memmove/memcpy logic in `copy_node_range` helper with `@unsafe` block that asserts `count <= width` and ranges do not overlap unexpectedly.
3. **RCU allocations** – return `RcuBox<T>` which automatically defers frees through `rcu::sync`. Provide `into_raw()` only for legacy paths.
4. **Callbacks & scans** – capture shared state via `rusty::Arc` or pass borrow tokens so callbacks cannot outlive nodes.

### When to use `@unsafe`
Allowed scenarios:
- Interfacing with pthread/RCU/system APIs not analyzable by RustyCpp.
- Copying raw bytes between nodes for performance (with documented proofs of non-overlap).
- Constructing objects in pre-allocated buffers (placement new) where constructors cannot run safely otherwise.

Every `@unsafe` region must state:
```
// SAFETY: e.g. child_ slots [p, p+n) are initialized, destination has capacity, versions frozen by lock.
```

### Testing & Tooling
- Borrow checker target (Phase 1) – run on every CI push touching Masstree paths.
- Functional tests – `masstree/testrunner`, `benchmarks/sto/masstree-beta/*`, `benchmarks/tpcc.cc` in safe mode.
- Stress & perf – existing scripts (`src/mako/benchmarks/sto/TRcu.cc`, `benchmarks/rpc_setup.cc`) plus new ones capturing throughput & latency.

---

