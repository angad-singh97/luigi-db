# Luigi Multi-Shard Mutex Double-Lock Bug Report

## Summary

**Bug**: `pthread_mutex_lock: Assertion 'e != ESRCH || !robust' failed` during coordinator shutdown in multi-shard configurations

**Severity**: Critical - causes coordinator crash

**Affects**: 2-shard and potentially higher shard count configurations

**Status**: Identified root cause

---

## Symptoms

1. **Low throughput** in 2-shard tests (~2.6 txns/sec vs expected ~10K txns/sec)
2. **Timeout warnings**: "Timeout waiting for in-flight transactions: 52/252 completed"
3. **RPC errors**: Multiple "DispatchAsync callback: RPC error=107" (Transport endpoint not connected)
4. **Crash**: `pthread_mutex_lock.c:438: __pthread_mutex_lock_full: Assertion 'e != ESRCH || !robust' failed`

---

## Root Cause Analysis

### The Problem

The pthread assertion `e != ESRCH || !robust` means:
- A thread is trying to lock a mutex whose owner thread has **terminated** (ESRCH = "No such process")
- This is a **use-after-free** scenario for thread-local mutexes

### Where It Happens

**File**: `src/deptran/luigi/coordinator.cc`

**Sequence of Events**:

1. **Benchmark completes** (line 230-234):
   ```cpp
   std::this_thread::sleep_for(std::chrono::seconds(config_.duration_sec));
   running_.store(false);
   for (auto &w : workers)
     w.join();  // Worker threads terminate here
   ```

2. **Worker threads terminate**, but RPC callbacks are still pending in the reactor/poll thread

3. **Timeout occurs** (line 236-252):
   - Coordinator waits 10 seconds for in-flight transactions
   - Many transactions don't complete due to slow multi-shard coordination
   - Timeout expires with 200 transactions still in-flight

4. **Coordinator destructor runs** (~line 158-161):
   ```cpp
   ~LuigiCoordinator() {
     Stop();
     StopOwdThread();
   }
   ```

5. **RPC callbacks fire AFTER worker threads terminated** (lines 397-450):
   ```cpp
   commo_->DispatchAsync(
       shard, req.txn_id, expected, worker_id, involved_i32, ops_data,
       [this, in_flight, ...](bool ok, ...) {
         // This callback runs on reactor thread
         // ...
         CompleteTransaction(in_flight, committed);  // Line 421 or 433
       });
   ```

6. **CompleteTransaction tries to lock terminated thread's mutex** (line 634):
   ```cpp
   void CompleteTransaction(std::shared_ptr<InFlightTxn> in_flight, bool committed) {
     // ...
     {
       std::lock_guard<std::mutex> lock(*thread_stats_[in_flight->tid].mutex);
       // ^^^ CRASH: thread_stats_[tid].mutex belongs to a terminated worker thread
       // ...
     }
   }
   ```

### Why It's Worse in Multi-Shard

- **More coordination overhead**: 2+ shards require watermark exchange and cross-shard coordination
- **Slower transaction completion**: Transactions take much longer (276ms avg vs 18ms for 1-shard)
- **More in-flight transactions at timeout**: 200 pending vs typically <10 for 1-shard
- **Higher probability of callbacks after shutdown**: More pending RPCs when coordinator terminates

---

## Technical Details

### The Mutex Ownership Issue

The `thread_stats_` vector contains per-thread statistics:
```cpp
struct ThreadStats {
  std::vector<TxnRecord> records;
  uint64_t committed = 0, aborted = 0;
  std::unique_ptr<std::mutex> mutex;  // ← Problem: owned by worker thread
  
  ThreadStats() : mutex(std::make_unique<std::mutex>()) {}
};
std::vector<ThreadStats> thread_stats_;
```

**The bug**: When worker thread `tid` terminates, its stack and thread-local storage are destroyed, but the mutex in `thread_stats_[tid]` is still accessible. When a callback tries to lock this mutex, pthread detects the owner thread no longer exists.

### Why 1-Shard 3-Replica Tests Succeeded

The 1-shard 3-replica tests we ran earlier **did complete successfully** with good results, but there were warning signs:
- "Killed" messages during cleanup (servers terminated forcefully)
- The bug manifests more severely with:
  - Higher shard counts (more cross-shard RPCs)
  - Longer-running transactions (more time for race conditions)
  - Higher thread counts (more mutexes to access)

---

## Proposed Fixes

### Option 1: Shared Mutex (Quick Fix)

Move the mutex out of thread-local storage:

```cpp
struct ThreadStats {
  std::vector<TxnRecord> records;
  uint64_t committed = 0, aborted = 0;
  // Remove per-thread mutex
};
std::vector<ThreadStats> thread_stats_;
std::mutex stats_mutex_;  // Single shared mutex for all threads

void CompleteTransaction(...) {
  std::lock_guard<std::mutex> lock(stats_mutex_);  // Use shared mutex
  thread_stats_[in_flight->tid].records.push_back(record);
  // ...
}
```

**Pros**: Simple, guaranteed to work
**Cons**: Contention on single mutex (but acceptable for stats recording)

### Option 2: Atomic Counters + Lock-Free Queue (Better Performance)

```cpp
struct ThreadStats {
  std::atomic<uint64_t> committed{0};
  std::atomic<uint64_t> aborted{0};
  // Use lock-free queue for records (e.g., boost::lockfree::queue)
};
```

**Pros**: No mutex contention, better performance
**Cons**: More complex, requires lock-free data structures

### Option 3: Graceful Shutdown (Most Robust)

Ensure all RPC callbacks complete before destroying coordinator:

```cpp
~LuigiCoordinator() {
  Stop();
  
  // Wait for all callbacks to complete
  while (dispatched_txns_.load() > completed_txns_.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  
  // Cancel any remaining RPCs
  commo_->CancelAllPending();
  
  StopOwdThread();
}
```

**Pros**: Cleanest solution, prevents all shutdown races
**Cons**: Requires RPC cancellation support, more code changes

---

## Recommended Fix

**Combination approach**:

1. **Immediate**: Apply Option 1 (shared mutex) to fix the crash
2. **Short-term**: Implement Option 3 (graceful shutdown) for robustness
3. **Long-term**: Consider Option 2 (lock-free) if stats recording becomes a bottleneck

### Implementation for Option 1

**File**: `src/deptran/luigi/coordinator.cc`

```cpp
// Around line 512 - change ThreadStats
struct ThreadStats {
  std::vector<TxnRecord> records;
  uint64_t committed = 0, aborted = 0;
  // Remove: std::unique_ptr<std::mutex> mutex;
};
std::vector<ThreadStats> thread_stats_;
std::mutex stats_mutex_;  // Add: shared mutex for all stats

// Around line 214 - remove per-thread mutex locking
for (size_t i = 0; i < thread_stats_.size(); i++) {
  // Remove: std::lock_guard<std::mutex> lock(*thread_stats_[i].mutex);
  thread_stats_[i].records.clear();
  thread_stats_[i].committed = thread_stats_[i].aborted = 0;
}

// Around line 462 - use shared mutex in CalculateStats
std::lock_guard<std::mutex> lock(stats_mutex_);
for (size_t i = 0; i < thread_stats_.size(); i++) {
  // Remove: std::lock_guard<std::mutex> lock(*thread_stats_[i].mutex);
  stats.committed_txns += thread_stats_[i].committed;
  // ...
}

// Around line 634 - use shared mutex in CompleteTransaction
void CompleteTransaction(std::shared_ptr<InFlightTxn> in_flight, bool committed) {
  uint64_t end = GetTimestampUs();
  
  {
    std::lock_guard<std::mutex> lock(stats_mutex_);  // Changed from per-thread mutex
    TxnRecord record;
    record.txn_id = in_flight->txn_id;
    record.start_time_us = in_flight->start_time_us;
    record.end_time_us = end;
    record.committed = committed;
    record.txn_type = in_flight->txn_type;
    thread_stats_[in_flight->tid].records.push_back(record);
    if (committed)
      thread_stats_[in_flight->tid].committed++;
    else
      thread_stats_[in_flight->tid].aborted++;
  }
  
  completed_txns_.fetch_add(1);
  completion_cv_.notify_all();
}
```

---

## Testing Plan

After applying the fix:

1. **Verify 2-shard tests work**: Run the 2-shard 3-replica tests that currently fail
2. **Regression test 1-shard**: Ensure 1-shard tests still pass
3. **Stress test**: Run with 4+ shards and high thread counts
4. **Valgrind/TSan**: Run with thread sanitizer to detect any remaining races

---

## Related Issues

- The slow throughput (2.6 txns/sec) in 2-shard tests is a **separate issue** from the mutex bug
- This appears to be a performance problem with multi-shard watermark coordination
- Should be investigated separately after fixing the crash

---

## Files Affected

- `src/deptran/luigi/coordinator.cc` (primary fix location)
- Potentially `src/deptran/luigi/commo.cc` (for graceful shutdown option)

---

## References

- Error log: `/root/cse532/mako/coord_micro.log`
- Test that triggered bug: `run_micro_2shard_3replicas_latency.sh 10 1 5 2 2 0.5`
- pthread error: `pthread_mutex_lock.c:438: __pthread_mutex_lock_full: Assertion 'e != ESRCH || !robust' failed`
