# Jetpack

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

### results process

```
python3 results_processor.py <Experiment time (directory name under results folder)>
```
e.g.
```
python3 results_processor.py 2023-10-10-03:38:03
```
