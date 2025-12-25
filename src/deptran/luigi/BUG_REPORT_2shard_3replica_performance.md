# 2-Shard 3-Replica Performance Bug Report

## Summary

**Bug**: 2-shard 3-replica configuration runs at 2.1 txns/sec (4000x slower than expected)

**Root Cause**: Follower sites not initialized, preventing replication quorum from being reached, which blocks watermark advancement and stalls all cross-shard transactions.

---

## Evidence

### Performance Comparison

| Configuration | Throughput | Status |
|--------------|------------|--------|
| 1-shard 3-replica | 10,429 txns/sec | ✅ Works |
| 2-shard 1-replica | 8,451 txns/sec | ✅ Works |
| 2-shard 3-replica | **2.1 txns/sec** | ❌ **Broken** |

### Symptoms

1. **Only 42/242 transactions completed** in 10 seconds
2. **Massive log spam**: 73,000 lines per leader log
3. **Watermark stuck at 0**: Shard 1 watermark never advances
4. **No replication quorum**: 0 "quorum reached" messages in logs
5. **Watermark exchange spam**: 981 exchanges, all showing watermark=0

### Log Analysis

```
# Leader logs are huge due to watermark spam
s101_micro.log: 13M (73,000 lines)
s201_micro.log: 13M (73,000 lines)

# Follower logs are tiny (barely used)
s102_micro.log: 234K
s103_micro.log: 2.5K
s202_micro.log: 2.3K
s203_micro.log: 2.4K

# Watermark exchanges every 100ms, but stuck at 0
HandleWatermarkExchange: shard 0 received from shard 1, watermark[0]=0
HandleWatermarkExchange: shard 0 received from shard 1, watermark[0]=0
... (repeated 981 times)

# Replication batches sent to followers
s102: BatchAppendToLog entries=16 + entries=26 = 42 total
s103: BatchAppendToLog entries=16 + entries=26 = 42 total

# But NO quorum reached messages!
grep "quorum reached" s201_micro.log => 0 results
```

---

## Root Cause

### The Problem

In `scheduler.cc` line 1065:
```cpp
if (num_replicas_ > 1 && !follower_sites_.empty() && commo_ != nullptr) {
```

**`follower_sites_` is EMPTY**, so the replication code path is never executed!

Without replication:
1. Watermarks never advance (line 1106: watermark update only happens in quorum callback)
2. Cross-shard transactions wait forever for watermarks
3. Only single-replica watermark update path runs (line 1126)
4. But that path is never reached because `follower_sites_.empty()` is true

### Why It Works for 1-Shard 3-Replica

- Single shard doesn't need cross-shard watermark coordination
- Local watermark advancement is sufficient
- Replication happens but cross-shard dependencies don't block

### Why It Works for 2-Shard 1-Replica

- No replication needed (`num_replicas_ = 1`)
- Watermarks advance immediately (line 1126 in single-replica path)
- Cross-shard coordination works because watermarks aren't blocked

---

## The Bug

**`follower_sites_` is never initialized for multi-shard configurations!**

Looking at the code:
- `follower_sites_` is a member variable in `SchedulerLuigi`
- It should be populated with follower site IDs during initialization
- But there's no `SetFollowerSites()` call in the multi-shard setup

---

## Fix Required

### Option 1: Initialize follower_sites_ from Config

In `SchedulerLuigi::SetPartitionId()` or similar initialization:

```cpp
void SchedulerLuigi::SetPartitionId(uint32_t shard_id) {
  shard_id_ = shard_id;
  executor_.SetPartitionId(shard_id);
  
  // FIX: Initialize follower sites from config
  auto config = Config::GetConfig();
  auto& partition_sites = config->SiteInfo(shard_id);
  
  for (size_t i = 1; i < partition_sites.size(); i++) {
    follower_sites_.push_back(partition_sites[i]);
  }
  
  num_replicas_ = partition_sites.size();
  Log_info("Initialized shard %u with %zu replicas, %zu followers",
           shard_id, num_replicas_, follower_sites_.size());
}
```

### Option 2: Add SetFollowerSites() Call

In the server initialization code (likely in `service.cc` or main):

```cpp
// After creating scheduler
scheduler->SetPartitionId(shard_id);
scheduler->SetFollowerSites(follower_site_ids);  // ADD THIS
```

---

## Testing Plan

After fix:
1. Run 2-shard 3-replica test again
2. Verify follower_sites_ is populated (check logs)
3. Verify "quorum reached" messages appear
4. Verify watermarks advance beyond 0
5. Verify throughput is comparable to 1-shard 3-replica (~10K txns/sec)

---

## Impact

**Critical**: Multi-shard replication is completely broken without this fix.

**Workaround**: Use 1-replica per shard (no replication) for multi-shard deployments.

---

## Files to Investigate

- `src/deptran/luigi/scheduler.cc` - Replication logic
- `src/deptran/luigi/service.cc` - Server initialization
- `src/deptran/luigi/server_main.cc` - Main entry point
- Config parsing code - Where follower sites should be extracted
