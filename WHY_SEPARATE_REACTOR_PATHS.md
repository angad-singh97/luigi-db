# Why We Need Separate Reactor Paths for Paxos and Raft

**A Technical Analysis of Two Different Event Models**

---

## Executive Summary

This document explains why the reactor implementation requires separate code paths for Paxos and Raft consensus protocols. The fundamental difference lies in how events signal their readiness: Paxos events rely on external state changes that require active polling, while Raft events can self-notify when ready. These two models are incompatible and cannot share the same reactor implementation without sacrificing either correctness (for Paxos) or performance (for Raft).

---

## The Fundamental Difference

### Paxos Events: Passive Condition Checking

Paxos events typically depend on external state that changes independently of the event itself. The event provides a condition to check (`IsReady()`), but has no mechanism to detect when that condition becomes true.

```cpp
// A Paxos event waiting for quorum (3 out of 5 replicas)
class QuorumEvent : public Event {
    int n_total = 5;
    int n_acks = 0;  // Updated by external code

    bool IsReady() override {
        return n_acks >= 3;  // Just checks the condition
    }
};

// Somewhere else in the codebase:
void OnPrepareResponse(int replica_id) {
    quorum_event->n_acks++;  // Increment counter
    // Event is not notified of this change
    // No call to Test()
    // Reactor must actively check
}
```

**Characteristics:**
- Event state modified by external code
- Event has no knowledge of state changes
- No notification mechanism
- Requires active polling to detect readiness

**Analogy:** A thermostat that reads temperature but doesn't know when temperature changes. Someone must continuously check the thermostat.

### Raft Events: Active Self-Notification

Raft events are designed such that the code making them ready has a direct reference to the event and can explicitly notify it.

```cpp
// A Raft event waiting for RPC response
class RPCResponseEvent : public Event {
    bool response_arrived = false;

    bool IsReady() override {
        return response_arrived;
    }
};

// RPC response handler has access to the event
void OnRPCResponse(RPCResponseEvent* event) {
    event->response_arrived = true;
    event->Test();  // Explicitly notify the event
}
```

**Inside Test():**
```cpp
bool Event::Test() {
    if (IsReady()) {
        status_ = READY;
        // Push to ready queue - notify reactor
        Reactor::GetReactor()->ready_events_.push_back(this);
    }
}
```

**Characteristics:**
- Code that modifies state has event reference
- Explicit notification via `Test()` call
- Event actively notifies reactor
- No polling required

**Analogy:** A doorbell that rings when pressed. No need to continuously check if someone is at the door.

---

## Implementation: Two Code Paths

### Path 1: Active Scanning (Paxos Mode)

When `ShouldTrackWaitingEvents()` returns `true`, the reactor actively scans all waiting events.

**Event Lifecycle:**

```
Step 1: Event::Wait() is called
        ↓
        Event added to waiting_events_ list

Step 2: Reactor::Loop() executes
        ↓
        Scans all events in waiting_events_
        Calls Test() on each event
        Moves READY events to ready_events_

Step 3: Event becomes ready (external state changed)
        ↓
        Next loop iteration detects it via scanning
        Event moved to ready_events_
        Coroutine resumes
```

**Code Implementation:**

```cpp
// Event::Wait() - event.cc lines 75-78
if (Reactor::ShouldTrackWaitingEvents()) {
    auto& waiting_events = Reactor::GetReactor()->waiting_events_;
    waiting_events.push_back(self);
}

// Event::Test() - event.cc lines 154-156
if (Reactor::ShouldTrackWaitingEvents()) {
    // Don't push to ready queue
    // Reactor will find us by scanning
}

// Reactor::Loop() - reactor.cc lines 252-268
if (ShouldTrackWaitingEvents()) {
    auto& events = waiting_events_;
    for (auto it = events.begin(); it != events.end();) {
        Event& event = **it;
        event.Test();  // Active check

        if (event.status_ == Event::READY) {
            ready_events.push_back(*it);
            it = events.erase(it);
            found_ready_events = true;
        }
    }
}
```

### Path 2: Passive Notification (Raft Mode)

When `ShouldTrackWaitingEvents()` returns `false`, the reactor relies on events to self-notify.

**Event Lifecycle:**

```
Step 1: Event::Wait() is called
        ↓
        Event optionally added to waiting_events_ (for debugging only)

Step 2: Reactor::Loop() executes
        ↓
        SKIPS scanning waiting_events_
        Only processes ready_events_ queue

Step 3: Event becomes ready (RPC arrives)
        ↓
        Handler calls event.Test()
        Test() pushes to ready_events_ queue
        Next loop iteration processes queue
        Coroutine resumes
```

**Code Implementation:**

```cpp
// Event::Wait() - event.cc lines 79-90
if (Reactor::ShouldTrackWaitingEvents()) {
    // Paxos path (above)
} else if (rcd_wait_) {
    // Only track if explicitly marked for debugging
    auto& waiting_events = Reactor::GetReactor()->waiting_events_;
    waiting_iter_ = waiting_events.insert(waiting_events.end(), self);
    in_waiting_list_ = true;
}

// Event::Test() - event.cc lines 158-165
else {
    // Remove from waiting list if present
    if (in_waiting_list_) {
        auto& waiting_events = Reactor::GetReactor()->waiting_events_;
        waiting_events.erase(waiting_iter_);
        in_waiting_list_ = false;
    }
    // Push to ready queue
    Reactor::GetReactor()->ReadyEventsThreadSafePushBack(shared_from_this());
}

// Reactor::Loop() - reactor.cc line 253
if (ShouldTrackWaitingEvents()) {
    // This entire scanning block is SKIPPED
}
```

---

## Real-World Examples from the Codebase

### Example 1: Paxos Prepare Phase (Requires Scanning)

```cpp
// Paxos coordinator sends Prepare to 5 replicas
void Prepare() {
    auto prepare_event = make_shared<QuorumEvent>();
    prepare_event->n_total = 5;
    prepare_event->n_acks = 0;

    // Send Prepare RPC to all replicas
    for (int i = 0; i < 5; i++) {
        SendPrepareRPC(i, [prepare_event](int replica_id) {
            // Callback when response arrives
            prepare_event->n_acks++;  // Just increment counter
            // No Test() call
            // Callback doesn't know when quorum is reached
        });
    }

    // Wait for majority (3 out of 5)
    prepare_event->Wait();  // Pauses coroutine
}
```

**Process Flow:**
1. Event added to `waiting_events_`
2. Reactor keeps calling `prepare_event->Test()` every loop iteration
3. Test() checks `n_acks >= 3`
4. When condition becomes true, event transitions to READY
5. Coroutine resumes

**Why scanning is required:**
- Callbacks only modify the counter
- Callbacks don't have knowledge of the quorum threshold
- No code knows when the "critical 3rd response" arrives
- Active polling is the only way to detect quorum

### Example 2: Raft AppendEntries RPC (Self-Notifying)

```cpp
// Raft leader sends AppendEntries to follower
void SendAppendEntries(int follower_id) {
    auto rpc_event = make_shared<RPCEvent>();
    rpc_event->response_arrived = false;

    // Send RPC and pass event to callback
    SendRPC(follower_id, [rpc_event](Response resp) {
        // Callback has direct access to event
        rpc_event->response_arrived = true;
        rpc_event->Test();  // Explicit notification
    });

    // Wait for response
    rpc_event->Wait();  // Pauses coroutine
}
```

**Process Flow:**
1. Event may or may not be added to `waiting_events_` (depends on `rcd_wait_`)
2. Reactor does NOT scan (ShouldTrackWaitingEvents() == false)
3. When response arrives, callback directly calls `Test()`
4. Test() pushes event to `ready_events_` queue
5. Reactor processes queue, coroutine resumes

**Why notification is sufficient:**
- Callback knows exactly when RPC completes
- Callback has reference to the specific event
- Direct notification is possible and efficient

---

## The RAFT_BATCH_MODE Flag: Runtime Mode Selection

The environment variable `RAFT_BATCH_MODE` controls which reactor mode is active:

```cpp
// reactor.cc lines 33-36
static std::atomic<bool> g_rrr_track_waiting_events{[]() {
    const char* env = getenv("RAFT_BATCH_MODE");
    return !(env && strcmp(env, "1") == 0);
}()};
```

**Mode Selection:**
```
Environment Variable       g_rrr_track_waiting_events    Reactor Mode
--------------------       --------------------------    ------------
Not set                    TRUE                          Paxos (scanning)
RAFT_BATCH_MODE=0         TRUE                          Paxos (scanning)
RAFT_BATCH_MODE=1         FALSE                         Raft (notification)
```

### Three Critical Decision Points

**Decision Point 1: Event::Wait() - Should we track this event?**

```cpp
// event.cc lines 75-90
if (Reactor::ShouldTrackWaitingEvents()) {
    // PAXOS: Add to waiting_events_ for scanning
    auto& waiting_events = Reactor::GetReactor()->waiting_events_;
    waiting_events.push_back(self);
} else if (rcd_wait_) {
    // RAFT: Only add if explicitly marked for debugging
    auto& waiting_events = Reactor::GetReactor()->waiting_events_;
    waiting_iter_ = waiting_events.insert(waiting_events.end(), self);
    in_waiting_list_ = true;
}
```

**Decision Point 2: Event::Test() - Should we push to ready queue?**

```cpp
// event.cc lines 154-166
if (Reactor::ShouldTrackWaitingEvents()) {
    // PAXOS: Don't push - reactor will find us by scanning
} else {
    // RAFT: Push ourselves to ready queue
    if (in_waiting_list_) {
        waiting_events.erase(waiting_iter_);
    }
    Reactor::GetReactor()->ReadyEventsThreadSafePushBack(shared_from_this());
}
```

**Decision Point 3: Reactor::Loop() - Should we scan waiting events?**

```cpp
// reactor.cc lines 252-268
if (ShouldTrackWaitingEvents()) {
    // PAXOS: Scan all waiting events
    for (auto it = waiting_events_.begin(); it != waiting_events_.end();) {
        Event& event = **it;
        event.Test();
        if (event.status_ == Event::READY) {
            ready_events.push_back(*it);
        }
    }
}
// RAFT: Skip this entire block
```

---

## Why These Models Cannot Coexist

### Scenario 1: Using Raft Mode for Paxos (Fails)

```bash
export RAFT_BATCH_MODE=1  # Disable scanning
./ci/ci.sh simplePaxos    # Run Paxos tests
```

**Failure Sequence:**
```
1. Paxos QuorumEvent starts waiting for 3 out of 5 acks
2. Event added to waiting_events_ (if rcd_wait_ is true)
3. Reactor Loop: ShouldTrackWaitingEvents() returns FALSE
4. Scanning is skipped
5. Response 1 arrives → n_acks++ (no notification)
6. Response 2 arrives → n_acks++ (no notification)
7. Response 3 arrives → n_acks++ (no notification)
8. Event never becomes READY
9. Coroutine deadlocks
10. Test hangs or times out
```

**Root Cause:**
- Paxos callbacks don't call `Test()`
- Event sits in waiting_events_ but is never scanned
- Quorum is reached but never detected
- System deadlocks

**Correctness Violation:** The consensus protocol cannot make progress.

### Scenario 2: Using Paxos Mode for Raft (Works but Inefficient)

```bash
export RAFT_BATCH_MODE=0  # Enable scanning
# Run Raft workload
```

**Execution Sequence:**
```
1. Raft RPCEvent starts waiting for response
2. Event added to waiting_events_
3. Reactor Loop: Continuously scanning all events
4. Checks event: IsReady()? No...
5. Checks event: IsReady()? No...
6. RPC response arrives
7. Callback calls event.Test()
8. Test() sees ShouldTrackWaitingEvents() == TRUE
9. Test() does NOT push to ready_events_
10. Next reactor scan finds event is ready
11. Event processed, coroutine resumes
```

**Result:** Correct but inefficient

**Performance Issues:**
- Reactor wastes CPU cycles scanning
- Events that can self-notify are still polled
- No benefit from Raft's explicit notification design
- Unnecessary overhead

**Conclusion:** This mode sacrifices the performance advantages that Raft's design provides.

---

## Performance Analysis

### Paxos Mode: Correctness Over Performance

**CPU Usage per Loop Iteration:**
```
✓ Get ready_events from queue          O(1)
✓ Scan ALL waiting_events_             O(N) where N = number of waiting events
✓ Check timeouts                       O(M) where M = number of timeout events
✓ Process ready_events                 O(K) where K = number of ready events

Total: O(N + M + K)
```

**Advantages:**
- Guaranteed to detect all ready events
- Works for events with external state changes
- No possibility of missed notifications
- Always makes progress

**Disadvantages:**
- CPU intensive when many events are waiting
- Checks events even when not ready
- Doesn't scale well with event count
- Wastes cycles on events that will notify themselves

### Raft Mode: Performance Over Simplicity

**CPU Usage per Loop Iteration:**
```
✓ Get ready_events from queue          O(1)
⏭ SKIP scanning                        (eliminated!)
✓ Check timeouts                       O(M) where M = number of timeout events
✓ Process ready_events                 O(K) where K = number of ready events

Total: O(M + K)
```

**Advantages:**
- CPU efficient (event-driven)
- Scales well with large numbers of events
- Only processes events when actually ready
- No wasted polling cycles

**Disadvantages:**
- Requires explicit `Test()` calls
- If notification is missed, event stuck forever
- Doesn't work for events with external state changes
- Requires more careful programming

---

## Design Philosophies

### Paxos Philosophy: Defensive Programming

```
Assumption: Events cannot be trusted to notify when ready
Strategy:   Actively verify all events every loop iteration
Trade-off:  Sacrifice performance for guaranteed correctness
```

**Characteristics:**
- Conservative approach
- Handles all event types uniformly
- No assumptions about event behavior
- Reliable but resource-intensive

### Raft Philosophy: Cooperative Programming

```
Assumption: Events will notify when ready
Strategy:   Wait for explicit notifications
Trade-off:  Sacrifice simplicity for performance
```

**Characteristics:**
- Optimistic approach
- Requires well-behaved events
- Assumes proper notification discipline
- Efficient but requires careful implementation

---

## Comparative Summary

| Aspect | Paxos Events | Raft Events |
|--------|-------------|-------------|
| **State Ownership** | External code | Event itself |
| **Notification** | Reactor polls | Event pushes |
| **Discovery Method** | Active scanning | Passive queue |
| **Code Coupling** | Loose (callbacks don't know events) | Tight (callbacks have event refs) |
| **Performance** | O(N) per loop | O(1) per loop |
| **Correctness Guarantee** | Always detects ready events | Relies on correct Test() calls |
| **Scalability** | Poor (linear with events) | Good (independent of event count) |
| **Programming Difficulty** | Simple | Requires discipline |

---

## Conclusion

The need for separate reactor code paths stems from fundamental differences in event notification models:

**Paxos events are passive:**
- State changes occur externally
- Events have no awareness of these changes
- Active polling is the only reliable detection method
- Scanning overhead is necessary for correctness

**Raft events are active:**
- Code that modifies state has event references
- Events can self-notify when ready
- Explicit notification eliminates polling overhead
- Performance benefits require careful programming

**These models are mutually exclusive:**
- Using notification-only mode for Paxos causes deadlocks (correctness violation)
- Using scanning mode for Raft wastes CPU cycles (performance penalty)

**The RAFT_BATCH_MODE flag provides the correct solution:**
- Runtime selection between modes
- Each protocol uses its optimal path
- No correctness compromises
- No unnecessary performance penalties

This is not a workaround or hack—it is the principled design for supporting two fundamentally different event models within a single reactor framework.
