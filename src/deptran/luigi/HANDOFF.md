# Luigi Infrastructure Handoff Document

## Current State (Dec 2024)

### Binaries
| Binary | Source | Purpose |
|--------|--------|---------|
| `luigi_server` | `server.cc` | Server process (leader/follower) |
| `luigi_coordinator` | `coordinator.cc` | Benchmark client with OWD measurement |

### Key Files
```
src/deptran/luigi/
├── coordinator.cc     # Benchmark + OWD (main entry point)
├── server.cc          # Server startup (main entry point)  
├── server.h           # LuigiServer class
├── commo.cc/h         # Role-aware RPC communication
├── scheduler.cc/h     # Timestamp ordering, agreement protocol
├── executor.cc/h      # Transaction execution, replication
├── service.cc/h       # RPC handlers
├── state_machine.cc/h # Tiga-style stored procedures
└── luigi_entry.h      # Log entry structures
```

### Connection Topology
```
      Coordinator
           │ dispatch, OWD pings
           ▼
     ┌─────┴─────┐
     ▼           ▼
  Leader_0 ◄──► Leader_1   (watermark, deadline propose/confirm)
     │           │
     ▼           ▼
  Follower    Follower     (replication from own leader)
```

**Each entity can connect to all others, but RPC usage follows protocol:**
- Coordinator → Leaders: `LuigiDispatch`, `OwdPing`
- Leader ↔ Leader: `DeadlinePropose`, `DeadlineConfirm`, `WatermarkExchange`
- Leader → Followers: Replication (via existing Mako mechanism)

---

## Future Work

### 1. Merge executor.cc + scheduler.cc → server.cc

**Rationale:**
- `scheduler.cc` and `executor.cc` contain server-specific logic
- Currently ~1200 lines combined
- Merging would make `server.cc` self-contained for all server behavior

**Analysis:**

| File | Lines | Purpose | Merge Complexity |
|------|-------|---------|------------------|
| `scheduler.cc` | ~900 | Timestamp ordering, agreement, thread management | Medium |
| `executor.cc` | ~300 | Execute ops, trigger replication | Low |
| `server.cc` | ~180 | Startup, config parsing | - |

**Approach:**
1. Move `SchedulerLuigi` class into `server.cc` (or `server_scheduler.cc`)
2. Move `LuigiExecutor` inline into scheduler
3. Keep `service.cc` separate (RPC handler wiring)
4. Keep `state_machine.cc` separate (reusable storage logic)

**Trade-offs:**
- Pro: Single file for server logic, easier to understand flow
- Con: Larger file (~1400 lines), harder to unit test individual components

### 2. Runtime Testing Required

**Not yet verified:**
- RPC connection establishment
- Dispatch → Server → Response flow
- Multi-shard agreement protocol
- Follower replication

**Test command:**
```bash
# Terminal 1: Start servers
./build/luigi_server -f src/deptran/luigi/config/local-2shard.yml -P s0 &
./build/luigi_server -f src/deptran/luigi/config/local-2shard.yml -P s1 &

# Terminal 2: Run benchmark
./build/luigi_coordinator -f src/deptran/luigi/config/local-2shard.yml -b micro -d 10
```

### 3. Config Format Verification

Current config assumes Mako's `janus::Config` format. Verify:
- Site role detection (leader = position 0 in shard list)
- Process name matching (`-P` flag)
- Host/port binding

---

## Code Pointers

### RPC Definitions
- `src/deptran/rcc_rpc.rpc` - Luigi service definition (lines ~166-200)
- Generated: `rcc_rpc.h` contains `LuigiProxy` and `LuigiService`

### Role-Aware Connection
- `commo.h:13-18` - `LuigiRole` enum
- `commo.cc:31-94` - `ConnectByRole()` implementation

### Transaction Flow (Server)
1. `service.cc:LuigiDispatch()` - RPC entry point
2. `scheduler.cc:LuigiDispatchFromRequest()` - Queue transaction
3. `scheduler.cc:HoldReleaseTd()` - Conflict detection, deadline assignment
4. `scheduler.cc:ExecTd()` - Execute when ready
5. `executor.cc:Execute()` - Run stored procedure
6. `executor.cc:TriggerReplication()` - Replicate to followers

### Agreement Protocol (Multi-shard)
1. `scheduler.cc:InitiateAgreement()` - Broadcast proposal
2. `commo.cc:BroadcastDeadlinePropose()` - Send to all leaders
3. `scheduler.cc:UpdateDeadlineRecord()` - Track responses
4. `scheduler.cc:SendRepositionConfirmations()` - Phase 2 if needed
