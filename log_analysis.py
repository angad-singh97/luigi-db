file_path = "curp.log"

lines = []
with open(file_path, "r") as file:
    lines = file.readlines()
    
QueryCoordinator_launch = []
QueryCoordinator_done = []

for line in lines:
    real_lines = line.split("src")
    for real_line in real_lines:
        if "fastpath fail, QueryCoordinator" in real_line:
            keyword_range = real_line[real_line.find("fastpath fail, QueryCoordinator") - 20 : real_line.find("fastpath fail, QueryCoordinator")]
            QueryCoordinator_launch.append(keyword_range[keyword_range.find("<"):keyword_range.find(">")+1])
        if "fastpath fail, OriginalProtocol" in real_line:
            keyword_range = real_line[real_line.find("fastpath fail, OriginalProtocol") - 20 : real_line.find("fastpath fail, OriginalProtocol")]
            QueryCoordinator_launch.append(keyword_range[keyword_range.find("<"):keyword_range.find(">")+1])
        if "QueryCoordinator success" in real_line:
            keyword_range = real_line[real_line.find("QueryCoordinator success") - 20 : real_line.find("QueryCoordinator success")]
            QueryCoordinator_done.append(keyword_range[keyword_range.find("<"):keyword_range.find(">")+1])
        if "Original Protocol success" in real_line:
            keyword_range = real_line[real_line.find("Original Protocol success") - 20 : real_line.find("Original Protocol success")]
            QueryCoordinator_done.append(keyword_range[keyword_range.find("<"):keyword_range.find(">")+1])

print(len(QueryCoordinator_launch))
print(len(QueryCoordinator_done))

for launch in QueryCoordinator_launch:
    if launch not in QueryCoordinator_done:
        print(launch)
        

