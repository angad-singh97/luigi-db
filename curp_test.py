#!/usr/bin/env python3
from subprocess import call
import subprocess
from time import time, sleep
import argparse
import resource
import os
import sys
from datetime import datetime
import psutil
import numpy as np

run_app_     = "build/deptran_server"
config_path_ = "config/"

now = datetime.now()
exp_dir = os.path.join("results", now.strftime("%Y-%m-%d-%H:%M:%S"))


LOCAL_FAST_PATH_TIMEOUT = 3
LOCAL_WAIT_COMMIT_TIMEOUT = 10
LOCAL_INSTANCE_COMMIT_TIMEOUT = 3

# WANWAIT_20_FAST_PATH_TIMEOUT = 25
# WANWAIT_20_WAIT_COMMIT_TIMEOUT = 45
# WANWAIT_20_INSTANCE_COMMIT_TIMEOUT = 45

TC_20_FAST_PATH_TIMEOUT = 25
TC_20_WAIT_COMMIT_TIMEOUT = 70
TC_20_INSTANCE_COMMIT_TIMEOUT = 100

modes_ = [
    "none_paxos",
    "none_mencius",
    "none_copilot",
    # "none_fpga_raft",
]
curp_modes_ = [
    "paxos_plus",
    # "mencius_plus",
    # "copilot_plus",
    # "fpga_raft_plus",
]
sites_ = [
    "12c1s3r1p",
]
benchmarks_ =  [
    "rw_1",
    "rw_1000",
    "rw_1000000"
]
concurrent_ = [
    "concurrent_1",
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
    # "concurrent_6",
    "concurrent_10",
    "concurrent_20",
    # "concurrent_30",
    "concurrent_40",
    # "concurrent_50",
    # "concurrent_60",
    # "concurrent_70",
    "concurrent_80",
    # "concurrent_90",
    # "concurrent_100",
    # "concurrent_120",
    "concurrent_150",
    # "concurrent_200",
    "concurrent_300",
    # "concurrent_500",
    "concurrent_600",
    # "concurrent_750",
    "concurrent_1000",
    "concurrent_2000",
    "concurrent_4000",
    "concurrent_9000",
    # "concurrent_10000",
    "concurrent_16000",
    # "concurrent_22000",
    "concurrent_30000",
]
finish_countdown_ = [
    1,
    10,
    100,
]
fast_path_timeout_ = [
    5,
    25
]
wait_commit_timeout_ = [
    i for i in range(5, 100, 20)
]
instance_commit_timeout_ = [
    i for i in range(5, 100, 20)
]

def run(latency, m, s, b, c, fc=0, to1=0, to2=0, to3=1000):
    pm = config_path_ + m + ".yml"
    ps = config_path_ + s + ".yml"
    pb = config_path_ + b + ".yml"
    pc = config_path_ + c + ".yml"

    output_path = os.path.join(exp_dir, str(latency) + 'ms-' + m + '-' + s + '-' + b + '-' + c + '-' + str(fc) + '-' + str(to1) + '-' + str(to2) + '-' + str(to3) + ".res")
    t1 = time()
    res = "INIT"
    try:
        with open(output_path, "w") as f:
            cmd = [run_app_, "-f", pm, "-f", ps, "-f", pb, "-f", pc, "-P", "localhost", "-d", "20", "-F", str(fc), "-O", str(to1)+ "-" + str(to2) + "-" + str(to3)]
            # print(' '.join(cmd))
            # r = call(cmd, stdout=f, stderr=f, timeout=60)
            process = subprocess.Popen(" ".join(cmd), shell=True, stdout=f, stderr=subprocess.STDOUT)
            sleep(10)
            cpu_usage = [[], [], [], []]
            for _ in range(10):
                cpu_percent = psutil.cpu_percent(interval=1, percpu=True)
                for cpu_id in range(3):
                    cpu_usage[cpu_id].append(cpu_percent[cpu_id])
                cpu_usage[3].append(np.sum(cpu_percent[3:]))
            process.wait(timeout=60)
            # res = "OK" if r == 0 else "Failed"
            f.write("\n")
            for cpu_id in range(3):
                text = "cpu" + str(cpu_id) + " " + str(cpu_usage[cpu_id]) + " medium: " + str(np.median(cpu_usage[cpu_id])) + " mean: " \
                    + str(np.mean(cpu_usage[cpu_id])) + " max: " + str(np.max(cpu_usage[cpu_id])) + "\n"
                f.write(text)
            text = "clientall " + str(cpu_usage[3]) + " medium: " + str(np.median(cpu_usage[3])) + " mean: " \
                    + str(np.mean(cpu_usage[3])) + " max: " + str(np.max(cpu_usage[3])) + "\n"
            f.write(text)
        res = "FINISH"
    except subprocess.TimeoutExpired:
        res = "Timeout"
    except Exception as e:
        print(e)
    t2 = time()
    print("%-15s%-10s%-15s%-20s%-6s \t %.2fs" % (m, s, b, c, res, t2-t1))

def timeout_finetune():
    benchmarks_ = ["rw_1000000"]
    finish_countdown_ = [10]
    fast_path_timeout_ = [5]
    wait_commit_timeout_ = [45]
    instance_commit_timeout_ = [45]

    exp_count = len(sites_) * len(curp_modes_) * len(latency_concurrent_) * len(benchmarks_) * len(finish_countdown_) \
        * len(fast_path_timeout_) * len(wait_commit_timeout_) * len(instance_commit_timeout_)
    # exp_count += len(sites_) * len(curp_modes_) * len(latency_concurrent_) * len(benchmarks_)
    estimate_minute = exp_count // 2
    estimate_hour = estimate_minute // 60
    estimate_minute -= estimate_hour * 60
    print("Number of total experiments is", exp_count)
    print("Estimate Finish Time is:" , estimate_hour, "h", estimate_minute, "min")

    print("%-15s%-10s%-15s%-20s%-6s \t %-5s" % ("mode", "site", "bench", "concurrent", "result", "time"))
    for s in sites_:
        for m in curp_modes_:
            for c in latency_concurrent_:
                for b in benchmarks_:
                    for fc in finish_countdown_:
                        for to1 in fast_path_timeout_:
                            for to2 in wait_commit_timeout_:
                                for to3 in instance_commit_timeout_:
                                    run(20, m, s, b, c, fc, to1, to2, to3)
    # for s in sites_:
    #     for m in modes_:
    #         for c in latency_concurrent_:
    #             for b in benchmarks_:
    #                 run(20, m, s, b, c)

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

    timeout_finetune()

if __name__ == "__main__":
    main()
