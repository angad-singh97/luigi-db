#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <cmath>
#include "mongodb_kv_table_handler.h"

namespace mongodb_handler {

class SampleCommand {
 public:
  int type, key, value;
};

class Distribution {
  std::vector<double> data_;
 public:
  void append(double x) {
    data_.push_back(x);
  }
  void merge(Distribution &o) {
    for (int i = 0; i < o.count(); i++)
      data_.push_back(o.data_[i]);
  }
  size_t count() {
    return data_.size();
  }
  double pct(double pct) {
    if (data_.size() == 0)
      return -1;
    sort(data_.begin(), data_.end());
    return data_[floor(data_.size() * pct)];
  }
  double pct50() {
    return pct(0.5);
  }
  double pct90() {
    return pct(0.9);
  }
  double pct99() {
    return pct(0.99);
  }
  double ave() {
    if (data_.size() == 0)
      return -1;
    double sum = 0;
    for (int i = 0; i < data_.size(); i++)
      sum += data_[i];
    return sum / data_.size();
  }
};

class MongodbConnectionThreadPool {

  using CallbackType = std::function<void(SampleCommand)>;

  int thread_num_;
  int round_robin_ = 0;

  std::vector<std::thread> threads_;
  // std::vector<CallbackType> callbacks_;
  std::vector<std::unique_ptr<std::mutex>> mtxs_;
  std::vector<std::unique_ptr<std::condition_variable>> conditions_;
  std::vector<std::shared_ptr<MongodbKVTableHandler>> mongodb_handlers_;
  std::vector<std::shared_ptr<Distribution>> durations_;

  std::vector<std::queue<SampleCommand>> request_queues_;

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
      SampleCommand cmd;
      {
        std::lock_guard<std::mutex> lk(*mtxs_[thread_id]);
        cmd = request_queues_[thread_id].front();
        request_queues_[thread_id].pop();
        // std::cout << "MongodbConnection unlock and start handle" << std::endl;
      }
      
      auto start_time = std::chrono::high_resolution_clock::now();

      if (cmd.type == 0) {
        // usleep(40000);
        mongodb_handlers_[thread_id]->Read(cmd.key);
      }
      else if (cmd.type == 1) {
        // usleep(40000);
        mongodb_handlers_[thread_id]->Write(cmd.key, cmd.value);
      }
      else
        break;

      auto end_time = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
      durations_[thread_id]->append(duration.count());

      // std::cout << "MongodbConnection handle finish" << std::endl;
      Callback(cmd);
      // std::cout << "MongodbConnection Callback finish" << std::endl;
    }
  }

  void MongodbRequest(SampleCommand cmd) {
    // std::cout << "MongodbRequest " << cmd.key << std::endl;
    {
        std::lock_guard<std::mutex> lk(*mtxs_[round_robin_]);
        request_queues_[round_robin_].push(cmd);
    }
    conditions_[round_robin_]->notify_one();

    round_robin_++;
    if (round_robin_ >= thread_num_)
      round_robin_ = 0;
  }

  static void Callback(SampleCommand cmd) {
    // std::cout << cmd.key << std::endl;
  }

  void Close() {
    SampleCommand terminate_cmd{-1, 0, 0};
    for (int i = 0; i < thread_num_; i++) {
      std::unique_lock<std::mutex> lk(*mtxs_[i]);
      request_queues_[i].push(terminate_cmd);
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

  MongodbConnectionThreadPool(int thread_num)
    : thread_num_(thread_num) {
    for (int i = 0; i < thread_num; i++) {
      // callbacks_.push_back(Callback);
      mtxs_.push_back(std::make_unique<std::mutex>());
      request_queues_.push_back(std::queue<SampleCommand>());
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