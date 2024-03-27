#include <chrono>
#include "mongocxx/instance.hpp"
#include "mongodb_kv_table_handler.h"
#include "mongodb_connection_thread_pool.h"

int main() {
    // mongocxx::instance instance;
    // mongodb_handler::MongodbKVTableHandler handler;
    // std::cout << handler.Read(1) << std::endl;
    // std::cout << handler.Write(1, 3) << std::endl;
    // std::cout << handler.Read(1) << std::endl;
    // std::cout << handler.Write(1, 5) << std::endl;
    // std::cout << handler.Read(1) << std::endl;
    // std::cout << handler.Write(3, 13) << std::endl;
    // std::cout << handler.Write(5, 15) << std::endl;
    // std::cout << handler.Read(3) << std::endl;

    mongodb_handler::MongodbConnectionThreadPool pool(100);

    srand(time(0));

    int total_num = 10000;

    auto start_time = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < total_num; i++) {
        mongodb_handler::SampleCommand cmd{rand() % 2, i, rand() % 1000000};
        pool.MongodbRequest(cmd);
    }

    pool.Close();

    auto end_time = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    std::cout << "Duration: " << duration.count() << " ms" << std::endl;
    std::cout << "Throughput: " << total_num * 1000.0 / duration.count()<< " req/s" << std::endl;
    std::cout << "Medium MongoDB Latency: " << pool.LatencyMs() << " ms" << std::endl;

    return 0;
}