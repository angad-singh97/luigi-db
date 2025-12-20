# Luigi Infrastructure Handoff Document

## Current State (Dec 2024) ✓

### Binaries
| Binary | Source | Purpose |
|--------|--------|---------|
| `luigi_server` | `server.cc` | Server process (both shards in one process) |
| `luigi_coordinator` | `coordinator.cc` | Benchmark client with OWD measurement |

### Key Files
```
src/deptran/luigi/
├── coordinator.cc     # Benchmark + OWD (main entry point)
├── server.cc/.h       # Two-phase server startup
├── commo.cc/.h        # All-to-all RPC communication
├── scheduler.cc/.h    # Timestamp ordering, agreement protocol
├── executor.cc/.h     # Transaction execution, replication
├── service.cc/.h      # RPC handlers
├── state_machine.cc/.h # Tiga-style stored procedures
└── config/local-2shard.yml # Test configuration
```

### Connection Topology
All entities connect to all other entities. RPC targeting at broadcast level:

| RPC | Sender | Targets |
|-----|--------|---------|
| `LuigiDispatch`, `OwdPing` | Coordinator | Leaders only |
| `DeadlinePropose/Confirm` | Leaders | Other leaders |
| `WatermarkExchange` | Leaders | Other leaders |
| Replication | Leaders | Own followers |

---

## Verified Working ✓

### Server Two-Phase Startup
```bash
./build/luigi_server -f src/deptran/luigi/config/local-2shard.yml -P localhost
```
**Output:**
```
=== Luigi Server ===
Process: localhost, Sites: 2
  - Site 0: shard=0 listening on 31850 ✓
  - Site 1: shard=1 listening on 31851 ✓
connect to 127.0.0.1:31850 success!
connect to 127.0.0.1:31851 success!
Luigi server ready for shard 0
Luigi server ready for shard 1
```

### Key Implementation Details
1. **Two-phase startup** (`server.cc`):
   - `StartListener()` - start RPC listener for each site
   - Brief pause (500ms) for all listeners to start
   - `ConnectAndStart()` - create commos and start schedulers

2. **Sync RPC methods** (`commo.cc`):
   - Use async RPC + `std::condition_variable` for blocking
   - Works from non-reactor threads (OWD, benchmark)

3. **Config format** (`local-2shard.yml`):
   - `proc_name=localhost` → `role=0` (leader) in Config parsing
   - Single process runs all sites

---

## Test Commands

```bash
# Terminal 1: Start server (both shards in one process)
./build/luigi_server -f src/deptran/luigi/config/local-2shard.yml -P localhost

# Terminal 2: Run benchmark
./build/luigi_coordinator -f src/deptran/luigi/config/local-2shard.yml -b micro -d 10
```

---

## Future Work

### 1. Convert Server Threads to Coroutines
Server uses `std::thread` for background loops:
- `execTd` - transaction execution
- `holdandReleaseTd` - conflict detection
- `exchangeWaterMarkTd` - watermark broadcast

**Recommendation:** Use `Coroutine::CreateRun()` (like Raft) for cleaner reactor integration.

### 2. Skip Self-Connection
Servers currently connect to themselves (unnecessary). Fix: skip own site_id in base `Communicator`.

### 3. Merge executor.cc + scheduler.cc → server.cc (Optional)
| File | Lines | Purpose |
|------|-------|---------|
| `scheduler.cc` | ~900 | Timestamp ordering, agreement |
| `executor.cc` | ~300 | Execute ops, replication |
| `server.cc` | ~160 | Startup, config |

**Trade-off:** Single file vs. larger codebase (~1400 lines).

---

## Code Pointers

### RPC Definitions
- [rcc_rpc.rpc](file:///root/cse532/mako/src/deptran/rcc_rpc.rpc) lines ~166-200

### Transaction Flow
1. `service.cc:LuigiDispatch()` → RPC entry
2. `scheduler.cc:LuigiDispatchFromRequest()` → queue
3. `scheduler.cc:HoldReleaseTd()` → conflict detection
4. `scheduler.cc:ExecTd()` → execute
5. `executor.cc:Execute()` → stored procedure
6. `executor.cc:TriggerReplication()` → replicate

### Agreement Protocol
1. `scheduler.cc:InitiateAgreement()`
2. `commo.cc:BroadcastDeadlinePropose()`
3. `scheduler.cc:UpdateDeadlineRecord()`
4. `scheduler.cc:SendRepositionConfirmations()`
