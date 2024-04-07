#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <cmath>
#include "mongodb_kv_table_handler.h"
#include "RW_command.h"

namespace janus {

// class MongodbConnectionThreadPool {

//   // using CallbackType = std::function<void(shared_ptr<Marshallable>)>;

//   int thread_num_;
//   int round_robin_ = 0;

//   std::vector<std::thread> threads_;
//   std::vector<std::shared_ptr<MongodbKVTableHandler>> mongodb_handlers_;
//   std::vector<std::shared_ptr<Distribution>> durations_;

//   std::vector<std::queue<shared_ptr<Marshallable>>> request_queues_;
//   std::vector<std::unique_ptr<std::mutex>> request_mtxs_;
//   std::vector<std::unique_ptr<std::condition_variable>> request_conditions_;
  
//   std::vector<std::set<uint64_t>> finished_set_;
//   std::vector<std::unique_ptr<std::mutex>> finished_mtxs_;
//   std::vector<std::unique_ptr<std::condition_variable>> finished_conditions_;

//   // CallbackType callback_;

// public:

//   void MongodbConnection(int thread_id) {
//     while (true) {
//       shared_ptr<Marshallable> cmd;

//       // wait for a request
//       {
//         std::unique_lock<std::mutex> lk(*request_mtxs_[thread_id]);
//         request_conditions_[thread_id]->wait(lk,
//           [this, thread_id]{
//             return !request_queues_[thread_id].empty(); 
//           });
//         cmd = request_queues_[thread_id].front();
//         request_queues_[thread_id].pop();
//       }
      
//       if (cmd == nullptr)
//         break;

//       SimpleRWCommand parsed_cmd = SimpleRWCommand(cmd);

//       auto start_time = std::chrono::high_resolution_clock::now();

//       if (parsed_cmd.IsRead()) {
//         // usleep(40000);
//         mongodb_handlers_[thread_id]->Read(parsed_cmd.key_);
//       }
//       else if (parsed_cmd.IsWrite()) {
//         // usleep(40000);
//         mongodb_handlers_[thread_id]->Write(parsed_cmd.key_, parsed_cmd.value_);
//       }
//       else
//         break;

//       auto end_time = std::chrono::high_resolution_clock::now();
//       auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
//       durations_[thread_id]->append(duration.count());

//       {
//         std::lock_guard<std::mutex> lk(*finished_mtxs_[thread_id]);
//         finished_set_[thread_id].insert(SimpleRWCommand::GetCombinedCmdID(cmd));
//       }
//       finished_conditions_[thread_id]->notify_one();

//       // callback_(cmd);
//     }
//   }

//   void MongodbRequest(const shared_ptr<Marshallable>& cmd) {
//     int thread_id = round_robin_;
//     round_robin_++;
//     if (round_robin_ >= thread_num_)
//       round_robin_ = 0;

//     // push request
//     {
//         std::lock_guard<std::mutex> lk(*request_mtxs_[thread_id]);
//         request_queues_[thread_id].push(cmd);
//     }
//     request_conditions_[thread_id]->notify_one();

//     // wait for request finish
//     uint64_t combined_cmd_id = SimpleRWCommand::GetCombinedCmdID(cmd);
//     {
//       std::unique_lock<std::mutex> lk(*finished_mtxs_[thread_id]);
//       finished_conditions_[thread_id]->wait(lk,
//         [this, thread_id, combined_cmd_id]{
//           return finished_set_[thread_id].find(combined_cmd_id) != finished_set_[thread_id].end(); 
//         });
//       finished_set_[thread_id].erase(combined_cmd_id);
//     }
//   }

//   void Close() {
//     for (int i = 0; i < thread_num_; i++) {
//       std::unique_lock<std::mutex> lk(*request_mtxs_[i]);
//       request_queues_[i].push(nullptr);
//       lk.unlock();
//       request_conditions_[i]->notify_one();
//     }
//     for (int i = 0; i < thread_num_; i++) {
//       threads_[i].join();
//     }
//   }

//   double LatencyMs() {
//     Distribution all;
//     for (int i = 0; i < thread_num_; i++) {
//       all.merge(*durations_[i]);
//     }
//     return all.pct50();
//   }

//   MongodbConnectionThreadPool(int thread_num)
//     : thread_num_(thread_num) {
//     for (int i = 0; i < thread_num; i++) {
//       mongodb_handlers_.push_back(std::make_shared<MongodbKVTableHandler>());
//       durations_.push_back(std::make_shared<Distribution>());

//       request_queues_.push_back(std::queue<shared_ptr<Marshallable>>());
//       request_conditions_.push_back(std::make_unique<std::condition_variable>());
//       request_mtxs_.push_back(std::make_unique<std::mutex>());
      
//       finished_set_.push_back(std::set<uint64_t>());
//       finished_conditions_.push_back(std::make_unique<std::condition_variable>());
//       finished_mtxs_.push_back(std::make_unique<std::mutex>());
//     }
//     for (int i = 0; i < thread_num; i++)
//       threads_.push_back(std::thread([this, i]() {
//         MongodbConnection(i);
//       }));
//   }

//   ~MongodbConnectionThreadPool() {

//   }
// };

class MongodbConnectionThreadPool {

  using CallbackType = std::function<void(shared_ptr<Marshallable>)>;

  int thread_num_;
  int round_robin_ = 0;

  std::vector<std::thread> threads_;
  std::vector<std::unique_ptr<std::mutex>> mtxs_;
  std::vector<std::unique_ptr<std::condition_variable>> conditions_;
  std::vector<std::shared_ptr<MongodbKVTableHandler>> mongodb_handlers_;
  std::vector<std::shared_ptr<Distribution>> durations_;

  std::vector<std::queue<shared_ptr<Marshallable>>> request_queues_;

  CallbackType callback_;

public:

  void MongodbConnection(int thread_id) {
    while (true) {
      {
        std::unique_lock<std::mutex> lk(*mtxs_[thread_id]);
        conditions_[thread_id]->wait(lk,
          [this, thread_id]{
            return !request_queues_[thread_id].empty(); 
          });
      }
      shared_ptr<Marshallable> cmd;
      {
        std::lock_guard<std::mutex> lk(*mtxs_[thread_id]);
        cmd = request_queues_[thread_id].front();
        request_queues_[thread_id].pop();
      }
      
      if (cmd == nullptr)
        break;

      SimpleRWCommand parsed_cmd = SimpleRWCommand(cmd);

      auto start_time = std::chrono::high_resolution_clock::now();

      if (parsed_cmd.IsRead()) {
        // usleep(40000);
        mongodb_handlers_[thread_id]->Read(parsed_cmd.key_);
      }
      else if (parsed_cmd.IsWrite()) {
        // usleep(40000);
        mongodb_handlers_[thread_id]->Write(parsed_cmd.key_, parsed_cmd.value_);
      }
      else
        break;

      auto end_time = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
      durations_[thread_id]->append(duration.count());

      callback_(cmd);
    }
  }

  void MongodbRequest(const shared_ptr<Marshallable>& cmd) {
    {
        std::lock_guard<std::mutex> lk(*mtxs_[round_robin_]);
        request_queues_[round_robin_].push(cmd);
    }
    conditions_[round_robin_]->notify_one();

    round_robin_++;
    if (round_robin_ >= thread_num_)
      round_robin_ = 0;
  }

  void Close() {
    for (int i = 0; i < thread_num_; i++) {
      std::unique_lock<std::mutex> lk(*mtxs_[i]);
      request_queues_[i].push(nullptr);
      lk.unlock();
      conditions_[i]->notify_one();
    }
    for (int i = 0; i < thread_num_; i++) {
      threads_[i].join();
    }
  }

  double LatencyMs() {
    Distribution all;
    for (int i = 0; i < thread_num_; i++) {
      all.merge(*durations_[i]);
    }
    return all.pct50();
  }

  MongodbConnectionThreadPool(int thread_num, CallbackType callback)
    : thread_num_(thread_num), callback_(callback) {
    for (int i = 0; i < thread_num; i++) {
      // callbacks_.push_back(Callback);
      mtxs_.push_back(std::make_unique<std::mutex>());
      request_queues_.push_back(std::queue<shared_ptr<Marshallable>>());
      conditions_.push_back(std::make_unique<std::condition_variable>());
      mongodb_handlers_.push_back(std::make_shared<MongodbKVTableHandler>());
      durations_.push_back(std::make_shared<Distribution>());
    }
    for (int i = 0; i < thread_num; i++)
      threads_.push_back(std::thread([this, i]() {
        MongodbConnection(i);
      }));
  }

  ~MongodbConnectionThreadPool() {

  }
};


}
