# Curp-Plus

## Run the experiments locally

### build

build rpc
```
bin/rpcgen --python --cpp src/deptran/rcc_rpc.rpc && python3 add_virtual.py
```

build src code
```
python3 waf configure build -J
```

### build for raft testing

```
python3 waf configure build -J --enable-raft-test
```

### run raft tests

```
build/deptran_server -f config/raft_lab_test.yml
```

### simple test

```
build/deptran_server -f config/none_raft.yml -f config/1c1s3r1p.yml -f config/rw.yml -f config/client_closed.yml -f config/concurrent_1.yml —d 30 -m 100 —P localhost
```

### run

```
python3 curp_test.py
```

### results process

```
python3 results_processor.py <Experiment time (directory name under results folder)>
```
e.g.
```
python3 results_processor.py 2023-10-10-03:38:03
```

## Code structure

### main functions

In `src/deptran` folder, for every `<protocol>`, there is a corresponding `<protocol>plus` folder contains almost same content with the `<protocol>` folder, except for some renaming and small modification on the `OnCommit` function.

Most part of the Curp are in `src/deptran/scheduler.cc`. `OnCurpDispatch` function is the beginning of a fastpath command, and this will trigger other functions like `CurpPrepare`, `CurpAccept` and `CurpCommit` as protocol logic. `OnCurpPrepare`, `OnCurpAccept`, `OnCurpCommit` are functions react to the corresponding sent messages.

`DBGet` and `DBPut` are two functions simulate application Get and Put. This two will store application level k-v table at `TxLogServer::kv_table_`.

`MakeNoOpCmd` and `MakeFinishCmd` are two functions used for make a No-Op and Finish cmd which has the same structure as the passed in cmd in `OnCurpDispatch` function.

### log structure

All the curp fastpath instances are stored in `TxLogServer::curp_log_cols_`, which is a map from `key` to `CurpPlusDataCol` structure.

`CurpPlusDataCol` is the main part of the log structure. There's a `slot_id` to `CurpPlusData` map at `logs_` which stores all the instances. Try to use functions in this class to access `logs_` more and not to use `logs_` directly to avoid some unexpected behaviors.

### statistics part

There're some part to help output the statistics.

`Distribution` is a class to help output the specific percentage position latency.

Path countings and latency countings (`Distribution`) are in `TxLogServer` class and will be used for output during decomposations of the objects. Some will be called/outputed at `s_main.cc`.

## Fastpath go through

### Fastpath success

```
TxLogServer::OnCurpDispatch
Communicator::CurpForwardResultToCoordinator
TxLogServer::OnCurpForward
TxLogServer::CurpCommit
TxLogServer::OnCurpCommit
```

### Fastpath fail but accept success

```
TxLogServer::OnCurpDispatch
Communicator::CurpForwardResultToCoordinator
TxLogServer::OnCurpForward
TxLogServer::CurpAccept
TxLogServer::OnCurpAccept
TxLogServer::CurpCommit
TxLogServer::OnCurpCommit
```

## Fastpath & Original Protocol switch go through

In `src/deptran/curp/coordinator.cc`.

Initially start fastpath by
```
CoordinatorCurp::BroadcastDispatch
```
. If fastpath success, then
```
CoordinatorClassic::End
```
, else 
```
CoordinatorCurp::QueryCoordinator
```
. If Query success, then
```
CoordinatorClassic::End
```
, else
```
CoordinatorCurp::OriginalProtocol();
```
.

## Others

There maybe some debug logs printed out at some debug versions. Too much debug logs will influence performace, so remove them when you need accuracy performance statistics.

For the configurations, `rw_1000000` means the key-value store workload with key range `[0, 1000000)`, which is default `rw` setting. `rw_1000` and `rw_1` as smaller key range workloads are used for high contensions tests.
