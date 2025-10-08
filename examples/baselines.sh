run_test1() {
    bash ./examples/test_1shard_no_replication.sh 1
    bash ./examples/test_1shard_no_replication.sh 4
    bash ./examples/test_1shard_no_replication.sh 8
    bash ./examples/test_1shard_no_replication.sh 16 

    echo -e "nthreads\tthroughput" >> results.txt

    # Process each log file
    for file in test_1shard_no_replication.sh_shard0-*.log; do
        nthreads=$(echo "$file" | sed 's/.*shard0-\([0-9]*\)\.log/\1/')
        throughput=$(grep "agg_throughput:" "$file" | awk '{print $2}')
        echo -e "${nthreads}\t${throughput}"
    done | sort -n -k1 >> results.txt

    # results on zoo-003
    #nthreads	throughput
    #1	61718.1
    #4	219789
    #8	422288
    #16	798361
}

run_test2() {
    # bash ./examples/test_1shard_replication.sh 1
    # bash ./examples/test_1shard_replication.sh 4
    # bash ./examples/test_1shard_replication.sh 8
    bash ./examples/test_1shard_replication.sh 16 

    echo -e "nthreads\tthroughput" >> results.txt

    # Process each log file
    for file in test_1shard_replication.sh_shard0-localhost-*.log; do
        nthreads=$(echo "$file" | sed 's/.*shard0-localhost-\([0-9]*\)\.log/\1/')
        throughput=$(grep "agg_throughput:" "$file" | awk '{print $2}')
        echo -e "${nthreads}\t${throughput}"
    done | sort -n -k1 >> results.txt

}

run_test3() {
    bash ./examples/test_2shard_no_replication.sh 1
    bash ./examples/test_2shard_no_replication.sh 4
    bash ./examples/test_2shard_no_replication.sh 8
    bash ./examples/test_2shard_no_replication.sh 16 

    echo -e "nthreads\tthroughput" >> results.txt

    # Get unique thread counts
    thread_counts=$(ls test_2shard_no_replication.sh_shard*-*.log 2>/dev/null | sed 's/.*-\([0-9]*\)\.log/\1/' | sort -nu)

    # Process each thread count
    for nthreads in $thread_counts; do
        # Sum throughput across all shards for this thread count
        total_throughput=0

        for file in test_2shard_no_replication.sh_shard*-${nthreads}.log; do
            if [ -f "$file" ]; then
                # Extract throughput from file and add to total
                throughput=$(grep "agg_throughput:" "$file" | awk '{print $2}')
                if [ ! -z "$throughput" ]; then
                    total_throughput=$(echo "$total_throughput + $throughput" | bc)
                fi
            fi
        done

        # Output result
        echo -e "${nthreads}\t${total_throughput}" >> results.txt

    done
}

#run_test1
#run_test2
run_test3