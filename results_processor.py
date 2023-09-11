import os

def sorting_key(item):
    return item[:4] 


directory_path = 'tmp/'

files = os.listdir(directory_path)

file_names = [file for file in files if os.path.isfile(os.path.join(directory_path, file))]

datas = []

for file_name in file_names:
    parts = file_name.split("-")
    # print(parts)
    mode = parts[0]
    site = parts[1]
    workload = parts[2]
    conc = int(parts[3][parts[3].find("_")+1:parts[3].find(".")])
    throughtput = None
    latency50pct = None
    latency90pct = None
    latency99pct = None
    fastpath_count = None
    coordinatoraccept_count = None
    original_count = None
    max_gap = None
    with open(os.path.join(directory_path, file_name), "r") as file:
        for line in file:
            if "Total throughtput is" in line:
                throughtput = float(line[line.find("is")+3:].strip())
            if "Latency-50pct is" in line:
                latency50pct = float(line[line.find("Latency-50pct is")+17:line.find("Latency-90pct is")-4])
                latency90pct = float(line[line.find("Latency-90pct is")+17:line.find("Latency-99pct is")-4])
                latency99pct = float(line[line.find("Latency-99pct is")+17:-4])
            if "plus" in mode:
                if "FastPath-count" in line:
                    # print(line[line.find("OriginalProtocol-count =")+25:])
                    fastpath_count = int(line[line.find("FastPath-count =")+16:line.find("CoordinatorAccept-count")])
                    coordinatoraccept_count = int(line[line.find("CoordinatorAccept-count =")+26:line.find("OriginalProtocol-count")])
                    original_count = int(line[line.find("OriginalProtocol-count =")+25:])
                if "loc_id_=0" in line and "curp_executed_committed_max_gap_" in line:
                    max_gap = int(line[line.find("gap_=")+5:line.find("curp_fast_path_success_count_")])
    # print(mode, site, workload, conc, throughtput, latency50pct, latency90pct, latency99pct, fastpath_count, coordinatoraccept_count, original_count, max_gap)
    datas.append((site, workload, mode, conc, throughtput, latency50pct, latency90pct, latency99pct, fastpath_count, coordinatoraccept_count, original_count, max_gap))

datas = sorted(datas, key=sorting_key)
# print(datas)
for data in datas:
    print(data)
    