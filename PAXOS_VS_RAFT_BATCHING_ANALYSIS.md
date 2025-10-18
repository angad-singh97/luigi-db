# Why Paxos Requires Sequential Read While Raft Can Use Batch Read

## Executive Summary

**The Core Issue:** Paxos requires **strict sequential processing** of requests with **blocking semantics**, while Raft can process requests in **parallel batches**. This is NOT just about batching multiple requests together - it's about the **semantics of how each request is processed**.

---

## The Key Differences

### handle_read_single() (Paxos Mode)

```cpp
// Lines 172-186: Read EXACTLY ONE packet at a time
int n_peek = block_read_in.peek(&packet_size, sizeof(i32));
if(n_peek == sizeof(i32)){
    // Read packet
    int pckt_bytes = block_read_in.chnk_read_from_fd(socket_, packet_size + sizeof(i32) - block_read_in.content_size());

    // If not complete, STOP and WAIT
    if(block_read_in.content_size() < packet_size + sizeof(i32)){
        return false;  // ← BLOCKS until complete
    }

    // Create coroutine for THIS ONE request
    Coroutine::CreateRun([...] () {
        handler(req, weak_this);

        // CRITICAL: Reset buffer ONLY after handler completes
        auto sconn = weak_this.lock();
        if (sconn) {
            sconn->block_read_in.reset();  // ← Line 215
        }
    });
}
```

**Key Characteristics:**
1. **Reads ONE packet at a time**
2. **BLOCKS** if packet incomplete (`return false`)
3. **Uses block_read_in buffer** (persistent across calls)
4. **Resets buffer INSIDE coroutine** after handler completes (line 215)
5. **Buffer is NOT reset** until current request handler finishes

### handle_read_batch() (Raft Mode)

```cpp
// Lines 259-276: Read ALL available packets in a loop
for (;;) {
    i32 packet_size;
    int n_peek = in_.peek(&packet_size, sizeof(i32));

    // If full packet available, read it
    if (n_peek == sizeof(i32) && in_.content_size() >= packet_size + sizeof(i32)) {
        verify(in_.read(&packet_size, sizeof(i32)) == sizeof(i32));
        auto req = rusty::Box<Request>(new Request());
        verify(req->m.read_from_marshal(in_, packet_size) == (size_t) packet_size);

        // Add to batch
        complete_requests.push_back(std::move(req));
    } else {
        break;  // No more complete packets
    }
}

// Lines 282-317: Create coroutines for ALL batched requests
for (auto& req : complete_requests) {
    Coroutine::CreateRun([handler, req = std::move(req), weak_this]() {
        handler(std::move(req), weak_this);
        // NO buffer reset here!
    });
}
```

**Key Characteristics:**
1. **Reads ALL available packets** in one shot
2. **Non-blocking** - reads what's available, processes them all
3. **Uses in_ buffer** (different from block_read_in)
4. **Creates ALL coroutines immediately**
5. **No per-request buffer management**

---

## Why Paxos REQUIRES Sequential Processing

### Reason 1: **Strict Request Ordering for Consensus**

Paxos operates in **sequential phases** for each slot:

```
Request 1: PREPARE → ACCEPT → COMMIT
           ↓
Request 2: PREPARE → ACCEPT → COMMIT  ← Can't start until Request 1 commits
           ↓
Request 3: PREPARE → ACCEPT → COMMIT
```

**If you batch:**
```
Request 1: PREPARE phase starts
Request 2: PREPARE phase starts  ← WRONG! Might get same slot number!
Request 3: PREPARE phase starts  ← WRONG! Slot conflict!
```

**From coordinator.h lines 41-43:**
```cpp
ballot_t curr_ballot_ = 1;  // Current ballot
slotid_t slot_id_ = 0;      // Current slot being agreed on
```

These are **shared state** across requests! If you process multiple requests in parallel, they'll **conflict on slot assignment**.

### Reason 2: **Blocking Buffer Semantics - Critical Discovery!**

Look at the **buffer management difference**:

**Sequential Mode:**
```cpp
// handle_read_single() line 215-216 (INSIDE coroutine callback):
auto sconn = weak_this.lock();
if (sconn) {
    sconn->block_read_in.reset();  // Reset AFTER handler completes
}
```

**Why this matters:**
1. `block_read_in` is **persistent** - survives across `handle_read_single()` calls
2. Buffer is **NOT cleared** until the handler finishes executing
3. **Next handle_read_single() call** will see the **same buffer state**
4. If handler is still running, buffer is LOCKED

**This creates a BLOCKING QUEUE:**
```
Time 0: Request 1 arrives → stored in block_read_in → handler starts
Time 1: handle_read_single() called again → buffer still has Request 1 → return false (block)
Time 2: Request 1 handler completes → buffer.reset() → buffer empty
Time 3: Request 2 arrives → stored in block_read_in → handler starts
```

**Batch Mode:**
```cpp
// handle_read_batch() line 299-301 (NO buffer reset):
Coroutine::CreateRun([handler, req = std::move(req), weak_this]() {
    handler(std::move(req), weak_this);
    // NO reset - buffer is already consumed during read!
});
```

**Why this is different:**
1. `in_` buffer is **consumed immediately** during the loop (line 266)
2. Each request gets its **own Request object** (line 265)
3. **All requests processed in parallel** - no blocking

### Reason 3: **State Machine Serialization**

Paxos maintains a **replicated state machine** where operations MUST be applied in order:

```
Leader receives:
  - Transfer $100 from Account A to B
  - Transfer $50 from Account A to C

If processed in parallel:
  - Both might see Account A has $150
  - Both transfers succeed
  - Account A ends up with -$0 (WRONG!)

If processed sequentially:
  - First transfer: A=$150 → A=$50, B=+$100
  - Second transfer: A=$50 → A=$0, C=+$50
  - Correct final state
```

### Reason 4: **Quorum Events Must Complete Before Next Request**

From your understanding of the reactor, Paxos uses **QuorumEvents** that need scanning:

```cpp
// Paxos Prepare phase (conceptual):
QuorumEvent prepare_quorum;
prepare_quorum.n_total = 5;
prepare_quorum.n_acks = 0;

// Send Prepare to all replicas
// Wait for majority
prepare_quorum.Wait();  // Blocks coroutine until quorum reached

// Only AFTER quorum, proceed to Accept phase
```

**If you batch multiple requests:**
- Multiple coroutines all wait on different QuorumEvents
- Reactor scans waiting_events_ - finds them all as "waiting"
- When responses come back, quorums might complete out of order
- But Paxos **requires in-order completion**!

---

## Why Raft CAN Use Batch Processing

### Reason 1: **Leader-Based Batching**

Raft has a **single leader** that can batch operations:

```
Leader collects:
  - Request 1
  - Request 2
  - Request 3

Leader creates ONE log entry with all 3:
  - Entry 42: [Req1, Req2, Req3]

Leader replicates Entry 42 to followers (one round of consensus)

After commit, apply all 3 requests
```

### Reason 2: **Explicit Test() Calls**

From your earlier understanding, Raft events **explicitly call Test()** when ready:

```cpp
void OnRaftResponse() {
    event->response_arrived = true;
    event->Test();  // Actively notifies reactor
}
```

This works fine in batch mode because:
- Each event independently notifies when ready
- No need for sequential scanning
- Reactor just processes the ready queue

### Reason 3: **Per-Request Independent State**

Raft requests don't share global state like `curr_ballot_` or `slot_id_`:

```cpp
// Each request has its own log entry
struct LogEntry {
    uint64_t index;      // Unique index
    uint64_t term;       // Leader's term
    Command command;     // The actual request
};
```

No conflicts when processing multiple requests in parallel!

### Reason 4: **Append-Only Log**

Raft's log is **append-only** - no conflicts:

```
Thread 1: Append Request 1 at index 100
Thread 2: Append Request 2 at index 101  ← No conflict!
Thread 3: Append Request 3 at index 102  ← No conflict!
```

Paxos slots might conflict:

```
Thread 1: Try to agree on slot 50 with Request 1
Thread 2: Try to agree on slot 50 with Request 2  ← CONFLICT! Same slot!
```

---

## The Buffer Management Mystery Solved

### block_read_in vs in_

**block_read_in (Paxos):**
```cpp
// Persistent buffer - survives across handle_read calls
ChunkMarshal block_read_in;

// Reading is incremental:
block_read_in.chnk_read_from_fd(socket_, bytes_needed);

// Buffer persists until explicitly reset:
block_read_in.reset();  // Only happens after handler completes!
```

**in_ (Raft):**
```cpp
// Standard buffer - consumed immediately
Marshal in_;

// Reading consumes the buffer:
in_.read_from_fd(socket_);           // Read from network
req->m.read_from_marshal(in_, size); // Consume bytes from in_

// Buffer is automatically drained during the loop
```

**This is the SMOKING GUN!**

The `block_read_in` buffer creates a **synchronization point**:
1. Only one request can occupy the buffer at a time
2. Next request can't be read until buffer is reset
3. Buffer reset happens INSIDE the coroutine callback
4. **Forces sequential processing!**

---

## What Happens When You Use Batch Mode for Paxos?

### Scenario: Two concurrent Paxos requests

```
Time 0: Request 1 arrives
Time 0: Request 2 arrives (right behind Request 1)

With handle_read_batch():
  - Both requests read into separate Request objects
  - Both create coroutines immediately
  - Both coroutines run in parallel

Coroutine 1:
  - slot_id_ = 0
  - curr_ballot_ = 1
  - Send Prepare(slot=0, ballot=1) to replicas

Coroutine 2 (running concurrently!):
  - slot_id_ = 0  ← SAME SLOT! BUG!
  - curr_ballot_ = 1  ← SAME BALLOT! BUG!
  - Send Prepare(slot=0, ballot=1) to replicas

Replicas receive TWO Prepare messages for slot 0:
  - Both from ballot 1
  - Different commands
  - UNDEFINED BEHAVIOR!
```

### What Actually Fails

1. **Slot number collision**: Multiple requests try to use the same slot
2. **Ballot number collision**: Multiple coordinators increment same ballot
3. **State machine ordering violated**: Requests applied out of order
4. **Quorum counting breaks**: Responses to different Prepares get mixed up
5. **Buffer corruption** (if block_read_in was used in batch mode)

---

## The Fundamental Difference

### Paxos: **Externalized Synchronization**

```
Network Layer:    Sequential packet reading (block_read_in)
                          ↓
RPC Layer:        One request at a time
                          ↓
Consensus Layer:  Sequential slot assignment
                          ↓
State Machine:    Sequential execution
```

**Every layer enforces ordering!**

### Raft: **Internalized Synchronization**

```
Network Layer:    Batch packet reading (in_)
                          ↓
RPC Layer:        Multiple requests in parallel
                          ↓
Consensus Layer:  Leader batches into single log entry
                          ↓
State Machine:    Batch execution (but ordered within entry)
```

**Synchronization happens at the leader, not in the network layer!**

---

## Answer to Your Professor

**Why does Paxos use sequential read instead of batch read?**

**Short Answer:**
Paxos requires strict sequential processing because:
1. Shared state (slot_id_, curr_ballot_) would conflict
2. Request ordering must be maintained for state machine consistency
3. The blocking buffer (`block_read_in`) provides request-level synchronization
4. QuorumEvents for different requests must complete in order

**Longer Answer:**
The difference is NOT about performance - it's about **correctness**. Paxos's consensus algorithm fundamentally requires that:
- Each request gets a unique slot number
- Requests are agreed upon sequentially
- The state machine applies operations in order

Batching would break these invariants because multiple concurrent coordinator coroutines would race on shared state.

Raft CAN batch because:
- The leader serializes requests into a log
- Each log entry has a unique index (no races)
- Synchronization happens at the leader, not network layer
- Multiple requests can be batched into ONE consensus round

**Technical Answer:**
The `block_read_in` buffer in sequential mode acts as a **semaphore** - only one request can be "in flight" at a time. The buffer is not reset until the handler completes (line 215), which ensures the next `handle_read_single()` call will block until the previous request finishes. This creates the serialization needed for Paxos's shared state access pattern.

In contrast, `handle_read_batch()` uses the `in_` buffer which is fully consumed during packet parsing, allowing immediate parallel processing - perfect for Raft's append-only log model.

---

## Could Paxos Be Modified to Support Batching?

**Yes, but you'd need to:**

1. **Add per-request state isolation**:
   ```cpp
   struct PaxosRequest {
       slotid_t my_slot;      // Not shared!
       ballot_t my_ballot;    // Not shared!
       QuorumEvent my_quorum; // Not shared!
   };
   ```

2. **Implement slot allocation protocol**:
   ```cpp
   slotid_t AllocateNextSlot() {
       return atomic_fetch_add(&global_slot_counter, 1);
   }
   ```

3. **Handle out-of-order quorum completion**:
   - Track which requests finished
   - Buffer completed requests until all earlier ones finish
   - Apply to state machine in order

But this is **essentially reimplementing Raft**! Which is why Raft exists - it's designed for batching from the ground up.

---

## Conclusion

**Paxos uses sequential read because:**
- The algorithm is designed for sequential consensus
- Shared state requires serialization
- Buffer management enforces ordering
- Correctness depends on strict sequencing

**Raft uses batch read because:**
- The algorithm is designed for leader-based batching
- Log entries are independent (append-only)
- Leader handles synchronization internally
- Performance benefits from batching
