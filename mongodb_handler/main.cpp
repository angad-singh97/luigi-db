#include "mongocxx/instance.hpp"
#include "mongodb_kv_table_handler.h"


int main() {
    mongocxx::instance instance;
    mongodb_handler::MongodbKVTableHandler handler;
    std::cout << handler.Read(1) << std::endl;
    std::cout << handler.Write(1, 3) << std::endl;
    std::cout << handler.Read(1) << std::endl;
    std::cout << handler.Write(1, 5) << std::endl;
    std::cout << handler.Read(1) << std::endl;
    return 0;
}