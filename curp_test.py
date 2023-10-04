#!/usr/bin/env python3
from subprocess import call
import subprocess
from time import time
import argparse
import resource
import os
import sys
from datetime import datetime

run_app_     = "build/deptran_server"
config_path_ = "config/"

now = datetime.now()
exp_dir = os.path.join("results", now.strftime("%Y-%m-%d-%H:%M:%S"))


LOCAL_FAST_PATH_TIMEOUT = 3
LOCAL_WAIT_COMMIT_TIMEOUT = 10
LOCAL_INSTANCE_COMMIT_TIMEOUT = 3

WANWAIT_20_FAST_PATH_TIMEOUT = 25
WANWAIT_20_WAIT_COMMIT_TIMEOUT = 45
WANWAIT_20_INSTANCE_COMMIT_TIMEOUT = 45

TC_20_FAST_PATH_TIMEOUT = 25
TC_20_WAIT_COMMIT_TIMEOUT = 70
TC_20_INSTANCE_COMMIT_TIMEOUT = 100

modes_ = [
    # "paxos_plus",
    "mencius_plus",
    # "copilot_plus",
    # "none_paxos",
    # "none_fpga_raft",
    "none_mencius",
    # "none_copilot",
    # "fpga_raft_plus",
]
sites_ = [
    "10c1s3r1p",
]
latency_sites_ = [
    "20c1s3r1p",
]
benchmarks_ =  [
    "rw_1",
    "rw_1000",
    "rw_1000000"
]
concurrent_ = [
    "concurrent_1",
    "concurrent_2",
    "concurrent_3",
    "concurrent_4",
    "concurrent_5",
    "concurrent_6",
    "concurrent_7",
    "concurrent_8",
    "concurrent_9",
    "concurrent_10",
    "concurrent_20",
    "concurrent_30",
    "concurrent_40",
    "concurrent_50",
]
latency_concurrent_ = [
    "concurrent_1",
    "concurrent_3",
    "concurrent_6",
    "concurrent_10",
    "concurrent_20",
    "concurrent_30",
    "concurrent_40",
    "concurrent_50",
    "concurrent_60",
    "concurrent_70",
    "concurrent_80",
    "concurrent_90",
    "concurrent_100",
    "concurrent_120",
    "concurrent_150",
    "concurrent_200",
]

def run(latency, m, s, b, c):
    pm = config_path_ + m + ".yml"
    ps = config_path_ + s + ".yml"
    pb = config_path_ + b + ".yml"
    pc = config_path_ + c + ".yml"

    output_path = os.path.join(exp_dir, str(latency) + 'ms-' + m + '-' + s + '-' + b + '-' + c + ".res")
    t1 = time()
    res = "INIT"
    try:
        f = open(output_path, "w")
        cmd = [run_app_, "-f", pm, "-f", ps, "-f", pb, "-f", pc, "-P", "localhost", "-d", "20"]
        # print(' '.join(cmd))
        r = call(cmd, stdout=f, stderr=f, timeout=60)
        res = "OK" if r == 0 else "Failed"
        # res = "OK"
    except subprocess.TimeoutExpired:
        res = "Timeout"
    except Exception as e:
        print(e)
    t2 = time()
    print("%-15s%-10s%-15s%-15s%-6s \t %.2fs" % (m, s, b, c, res, t2-t1))
    pass

def main():
    global modes_
    global sites_
    global benchmarks_
    soft,hard = resource.getrlimit(resource.RLIMIT_NOFILE)
    if soft < 4096:
        print("open file limit smaller than 4096; set it with ulimit -n")
        sys.exit(0)
    
    if not os.path.exists(exp_dir):
        os.mkdir(exp_dir)

    # parser = argparse.ArgumentParser()
    # parser.add_argument('-m', '--mode', help='running modes', default=modes_,
    #                     nargs='+', dest='modes')
    # parser.add_argument('-s', '--site', help='sites', default=sites_,
    #                     nargs='+', dest='sites')
    # parser.add_argument('-b', '--bench', help='sites', default=benchmarks_,
    #                     nargs='+', dest='benchmarks')
    # args = parser.parse_args()
    # modes_ = args.modes
    # sites_ = args.sites
    # benchmarks_ = args.benchmarks

    print("%-15s%-10s%-15s%-15s%-6s \t %-5s" % ("mode", "site", "bench", "concurrent", "result", "time"))
    for s in latency_sites_:
        # for c in latency_concurrent_:
        for c in ["concurrent_1"]:
            for m in modes_:
                for b in benchmarks_ if "plus" in m else ["rw_1000000"]:
                    run(20, m, s, b, c)
    pass

def rpc_bd():
    output_path = os.path.join(exp_dir, 'rpc_bd.log')
    try:
        f = open(output_path, "w")

        rpc_bd = "bin/rpcgen --python --cpp src/deptran/rcc_rpc.rpc && python3 add_virtual.py"
        r = call(rpc_bd, stdout=f, stderr=f, timeout=120)
        if r == 0:
            print("rpc build success")
        else:
            print("rpc build fail")

    except subprocess.TimeoutExpired:
        print("rpc build / build timeout")
    except Exception as e:
        print(e)

def bd(fastpath_timeout, wait_commit_timeout, instance_commit_timeout):
    output_path = os.path.join(exp_dir, 'bd.log')
    try:
        f = open(output_path, "w")
        
        bd = "python3 waf configure build"
        r = call(bd, stdout=f, stderr=f, timeout=120)
        if r == 0:
            print("build success")
        else:
            print("build fail")

    except subprocess.TimeoutExpired:
        print("rpc build / build timeout")
    except Exception as e:
        print(e)


if __name__ == "__main__":
    main()
