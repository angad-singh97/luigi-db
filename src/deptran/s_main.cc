
#include "__dep__.h"
#include "frame.h"
#include "client_worker.h"
#include "procedure.h"
#include "command_marshaler.h"
#include "benchmark_control_rpc.h"
#include "server_worker.h"
#include "../rrr/reactor/event.h"
#include "scheduler.h"

// #define CPU_PROFILE 1

#ifdef CPU_PROFILE
# include <gperftools/profiler.h>
#endif // ifdef CPU_PROFILE

using namespace janus;

static ClientControlServiceImpl *ccsi_g = nullptr;
static rrr::PollMgr *cli_poll_mgr_g = nullptr;
static rrr::Server *cli_hb_server_g = nullptr;

static vector<ServerWorker> svr_workers_g = {};
vector<unique_ptr<ClientWorker>> client_workers_g = {};
static std::vector<std::thread> client_threads_g = {}; // TODO remove this?
static std::vector<std::thread> failover_threads_g = {};
bool* volatile failover_triggers;
volatile bool failover_server_quit = false;
volatile locid_t failover_server_idx;
volatile double total_throughput = 0;
int fastpath_count = 0;
int coordinatoraccept_count = 0;
int original_protocol_count = 0;
int fastpath_attempted_count = 0;
int fastpath_successed_count = 0;
Distribution cli2cli[7];
Distribution commit_time;
Frequency frequency;
// definition of first 4 elements refer to "Distribution cli2cli_[4];" in coordinator.h
// 5nd element is for merge first 4
#ifdef LATENCY_DEBUG
  Distribution client2leader, client2leader_send, client2test_point;
#endif

void client_setup_heartbeat(int num_clients) {
  Log_info("%s", __FUNCTION__);
  std::map<int32_t, std::string> txn_types;
  Frame* f = Frame::GetFrame(Config::GetConfig()->tx_proto_);
  f->GetTxTypes(txn_types);
  delete f;
  bool hb = Config::GetConfig()->do_heart_beat();
  if (hb) {
    // setup controller rpc server
    ccsi_g = new ClientControlServiceImpl(num_clients, txn_types);
    int n_io_threads = 1;
    cli_poll_mgr_g = new rrr::PollMgr(n_io_threads);
    base::ThreadPool *thread_pool = new base::ThreadPool(1);
    cli_hb_server_g = new rrr::Server(cli_poll_mgr_g, thread_pool);
    cli_hb_server_g->reg(ccsi_g);
    auto ctrl_port = std::to_string(Config::GetConfig()->get_ctrl_port());
    std::string server_address = std::string("0.0.0.0:").append(ctrl_port);
    Log_info("Start control server on port %s", ctrl_port.c_str());
    cli_hb_server_g->start(server_address.c_str());
  }
}

void client_launch_workers(vector<Config::SiteInfo> &client_sites) {
  // load some common configuration
  // start client workers in new threads.
  Log_info("client enabled, number of sites: %d", client_sites.size());
  vector<ClientWorker*> workers;

  failover_triggers = new bool[client_sites.size()]() ;
#ifdef SIMULATE_WAN
  int core_id = 5; // [JetPack] usually run within 5 replicas, 5 + 3 cores are enough for aws test
#endif
#ifndef SIMULATE_WAN
  int core_id = 1; // [JetPack] usually run 1 replica on each process on cloud setting
#endif
  for (uint32_t client_id = 0; client_id < client_sites.size(); client_id++) {
    ClientWorker* worker = new ClientWorker(client_id,
                                            client_sites[client_id],
                                            Config::GetConfig(),
                                            ccsi_g, nullptr, 
                                            &(failover_triggers[client_id]),
                                            &failover_server_quit,
                                            &failover_server_idx,
                                            &total_throughput);
    workers.push_back(worker);
    auto th_ = std::thread(&ClientWorker::Work, worker);
#ifndef AWS
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    int rc = pthread_setaffinity_np(th_.native_handle(),
                                    sizeof(cpu_set_t), &cpuset);
    if (rc != 0) {
      std::cerr << "Error calling pthread_setaffinity_np: " << rc << "\n";
    } else {
      Log_info("start a client thread on core %d, client-id:%d", core_id, client_id);
    }
    core_id ++;
#endif
    client_threads_g.push_back(std::move(th_));
    client_workers_g.push_back(std::unique_ptr<ClientWorker>(worker));
  }

}

// start servers in new threads.
void server_launch_worker(vector<Config::SiteInfo>& server_sites) {
  auto config = Config::GetConfig();
  Log_info("server enabled, number of sites: %d", server_sites.size());
  svr_workers_g.resize(server_sites.size(), ServerWorker());
  int i=0;
  vector<std::thread> setup_ths;
  int core_id = 0;
  for (auto& site_info : server_sites) {
    auto th_ = std::thread([&site_info, &i, &config] () {
      Log_info("launching site: %x, bind address %s",
               site_info.id,
               site_info.GetBindAddress().c_str());
      auto& worker = svr_workers_g[i++];
      worker.site_info_ = const_cast<Config::SiteInfo*>(&config->SiteById(site_info.id));
      Log_info("start SetupBase");
      worker.SetupBase();
      // register txn piece logic
      Log_info("start RegisterWorkload");
      worker.RegisterWorkload();
      // populate table according to benchmarks
      worker.PopTable();
      Log_info("table popped for site %d", (int)worker.site_info_->id);
#ifdef DB_CHECKSUM
      worker.DbChecksum();
#endif
      // start server service
      worker.tx_sched_->svr_workers_g = &svr_workers_g;
      if (worker.rep_sched_)
        worker.rep_sched_->svr_workers_g = &svr_workers_g;
      // worker.curp_rep_sched_->svr_workers_g = &svr_workers_g;
      worker.SetupService();
      Log_info("start communication for site %d", (int)worker.site_info_->id);
      worker.SetupCommo();
      Log_info("site %d launched!", (int)site_info.id);
      worker.launched_ = true;
    });
#ifndef AWS
    // for better performance, bind each server thread to a cpu core
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    int rc = pthread_setaffinity_np(th_.native_handle(),
                                    sizeof(cpu_set_t), &cpuset);
    if (rc != 0) {
      std::cerr << "Error calling pthread_setaffinity_np: " << rc << "\n";
    } else {
      Log_info("start a server thread on core %d, site-id:%d", core_id, site_info.id);
    }
    core_id ++;
#endif
    setup_ths.push_back(std::move(th_));
  }

  for (auto& worker : svr_workers_g) {
    while (!worker.launched_) {
      sleep(1);
    }
  }

  Log_info("waiting for server setup threads.");
  for (auto& th: setup_ths) {
    th.join();
  }
  Log_info("done waiting for server setup threads.");

  for (ServerWorker& worker : svr_workers_g) {
    // start communicator after all servers are running
    // setup communication between controller script
    worker.SetupHeartbeat();
  }
  Log_info("server workers' communicators setup");
}

void client_shutdown() {
  for (const unique_ptr<ClientWorker>& client: client_workers_g) {
    client->retrive_statistic();
    for (int i = 0; i < 6; i++)
      cli2cli[i].merge(client->cli2cli_[i]);
    frequency.merge(client->frequency_);
    commit_time.merge(client->commit_time_);
#ifdef LATENCY_DEBUG
    client2leader.merge(client->client2leader_);
    client2test_point.merge(client->client2test_point_);
    client2leader_send.merge(client->client2leader_send_);
#endif
    fastpath_count += client->fastpath_count_;
    coordinatoraccept_count += client->coordinatoraccept_count_;
    original_protocol_count += client->original_protocol_count_;

    fastpath_attempted_count += client->fastpath_attempted_count_;
    fastpath_successed_count += client->fastpath_successed_count_;
  }
  client_workers_g.clear();
}

void server_shutdown() {
  Log_info("server_shutdown");
  for (auto &worker : svr_workers_g) {
    worker.ShutDown();
  }
}

void check_current_path() {
  auto path = boost::filesystem::current_path();
  Log_info("PWD : ", path.string().c_str());
}

void wait_for_clients() {
  Log_info("%s: wait for client threads to exit.", __FUNCTION__);
  for (auto &th: client_threads_g) {
    th.join();
  }
}

void server_failover_co(bool random, bool leader, int srv_idx)
{
#ifdef FAILOVER_DEBUG
    Log_info("!!!!!!!!!!!!!!!enter server_failover_co");
#endif
    int idx = -1 ;
    int expected_idx = -1 ;
    int run_int = Config::GetConfig()->get_failover_run_interval() ;
    int stop_int = Config::GetConfig()->get_failover_stop_interval() ;

    if (srv_idx != -1)
    {
        expected_idx = srv_idx ;
    }
    else if(!random)
    {
        // temporary solution, assign id 0 as leader, id 1 as follower
        if(leader)
        {
            expected_idx = 0 ;
        }
        else
        {
            expected_idx = 1 ;
        }
    }
    else
    {
        // do nothing
    }

    for(int i=0;i<svr_workers_g.size();i++)
    {
      Log_debug("failover at index %d, id %d, loc id %d part id %d", 
        i, svr_workers_g[i].site_info_->id,
        svr_workers_g[i].site_info_->locale_id,  
        svr_workers_g[i].site_info_->partition_id_ );
    }

    for(int i=0;i<svr_workers_g.size();i++)
    {
        if(svr_workers_g[i].site_info_->locale_id == expected_idx )
        {
            idx = i ;
            break ;
        }
    }    
#ifdef FAILOVER_DEBUG
    Log_info("!!!!!!!!!!!!!!!!! failover_server_quit %d", failover_server_quit);
#endif
    while(!failover_server_quit)
    {
        if(random)
        {
            idx = rand() % svr_workers_g.size() ;
        }
        failover_server_idx = idx ;
#ifdef FAILOVER_DEBUG
        Log_info("!!!!!!!!!!!!!!!!!!!! before run_int wait %d s", run_int);
#endif
        sleep(run_int) ;
        // auto r = Reactor::CreateSpEvent<TimeoutEvent>(run_int * 1000 * 1000);
        // r->Wait();
#ifdef FAILOVER_DEBUG
        Log_info("!!!!!!!!!!!!!!!!!!!! after run_int wait");
#endif
        if(idx == -1) 
        {
          // TODO other types
          if (!leader) break ;
          idx = 0 ;
        }        
        if(failover_server_quit)
        {
#ifdef FAILOVER_DEBUG
          Log_info("!!!!!!!!!!!!!!!!!!!! quit here");
#endif
          break ;
        }
        for (int i = 0; i < client_workers_g.size() ; ++i)
        {
          failover_triggers[i] = true ;
        }
        // for (int i = 0; i < client_workers_g.size() ; ++i)
        // {
        //   while(failover_triggers[i]) {
        //     if (failover_server_quit) {
        //       Log_info("!!!!!!!!!!!!!!!!!!!! quit here");
        //       return ;
        //     }
        //   }
        // }
        // TODO the idx of client
#ifdef FAILOVER_DEBUG
        Log_info("!!!!!!!!!!!!!!before pause 0");
#endif
        client_workers_g[0]->Pause(idx) ;
        Log_info("!!!!!!!!!!!!!! failover paused");
//        svr_workers_g[idx].Pause() ;
        for (int i = 0; i < client_workers_g.size() ; ++i)
        {
          failover_triggers[i] = true ;
        }
#ifdef FAILOVER_DEBUG
        Log_info("!!!!!!!!!!!!!! before stop_int wait");
#endif
        Log_info("server %d paused for failover test", idx);
        sleep(stop_int) ;
        // auto s = Reactor::CreateSpEvent<TimeoutEvent>(stop_int * 1000 * 1000);
        // s->Wait() ;      
#ifdef FAILOVER_DEBUG  
        Log_info("!!!!!!!!!!!!!! after stop_int wait");
#endif
        // for (int i = 0; i < client_workers_g.size() ; ++i)
        // {
        //   while(failover_triggers[i]) {
        //     if (failover_server_quit) return ;
        //   }
        // }        
#ifdef FAILOVER_DEBUG
        Log_info("!!!!!!!!!!!!!!before resume 0");
#endif
        client_workers_g[0]->Resume(idx) ;
        Log_info("!!!!!!!!!!!!!! failover resumed");
//        svr_workers_g[idx].Resume() ;
#ifdef FAILOVER_DEBUG
        Log_info("server %d resumed for failover test", idx);
#endif
        if(leader)
        {
          // get current leader
          idx = failover_server_idx ;
        }
    }

}

void server_failover_thread(bool random, bool leader, int srv_idx) {
#ifdef FAILOVER_DEBUG
  Log_info("!!!!!!!!!!!!!!!enter server_failover_thread");
#endif
  Coroutine::CreateRun([&, random, leader, srv_idx]() { 
    server_failover_co(random, leader, srv_idx) ;
  }) ;
}

void server_failover()
{
    bool failover = Config::GetConfig()->get_failover();
    bool random = Config::GetConfig()->get_failover_random() ;
    bool leader = Config::GetConfig()->get_failover_leader() ;
    int idx = Config::GetConfig()->get_failover_srv_idx() ;
    if(failover)
    {
      /*Coroutine::CreateRun([&, random, leader, idx]() { 
        server_failover_co(random, leader, idx) ;
      }) ;*/
      // TODO only consider the partition 0 now
      failover_threads_g.push_back(
          std::thread(&server_failover_thread, random, leader, idx)) ;
    }
}

void setup_ulimit() {
  struct rlimit limit;
  /* Get max number of files. */
  if (getrlimit(RLIMIT_NOFILE, &limit) != 0) {
    Log_fatal("getrlimit() failed with errno=%d", errno);
  }
  Log_info("ulimit -n is %d", (int)limit.rlim_cur);
}

int main(int argc, char *argv[]) {
  check_current_path();
  Log_info("starting process %ld", getpid());
  setup_ulimit();

  // read configuration
  int ret = Config::CreateConfig(argc, argv);
  if (ret == SUCCESS) {
    Log_info("Read config finish");
  } else {
    Log_fatal("Read config failed");
    return ret;
  }

  auto client_infos = Config::GetConfig()->GetMyClients();
  if (client_infos.size() > 0) {
    client_setup_heartbeat(client_infos.size());
  }

#ifdef CPU_PROFILE
  char prof_file[1024];
  Config::GetConfig()->GetProfilePath(prof_file);
  // start to profile
  ProfilerStart(prof_file);
  Log_info("started to profile cpu");
#endif // ifdef CPU_PROFILE

  auto server_infos = Config::GetConfig()->GetMyServers();
  if (!server_infos.empty()) {
    server_launch_worker(server_infos);
    server_failover() ;
  } else {
    Log_info("no servers on this process");
  }

  if (!client_infos.empty()) {
    //client_setup_heartbeat(client_infos.size());
#ifdef AWS
    sleep(15); // [JetPack] This is add for aws server test, otherwise client may start when server not ready (like *** verify failed: commo_ != nullptr at ../src/deptran/copilot/frame.cc, line 82)
#endif
    client_launch_workers(client_infos);
    sleep(Config::GetConfig()->duration_);
    wait_for_clients();
    failover_server_quit = true;
    Log_info("all clients have shut down.");
  }
  // Log_info("Total throughtput is %.2f, Latency-50% is %.2f ms, Latency-90% is %.2f ms, Latency-99% is %.2f ms ", total_throughput, cli2cli.pct50(), cli2cli.pct90(), cli2cli.pct99());
  Log_info("Total throughtput is %.2f", total_throughput);
#ifdef DB_CHECKSUM
  sleep(90); // hopefully servers can finish hanging RPCs in 90 seconds.
#endif

  sleep(10); // hopefully servers can finish reset of work in 10 seconds

  for (auto& worker : svr_workers_g) {
    worker.WaitForShutdown();
  }

  Log_info("After worker.WaitForShutdown();");

  for (auto& ft : failover_threads_g) {
    ft.join();
  }

#ifdef DB_CHECKSUM
  map<parid_t, vector<int>> checksum_results = {};
  for (auto& worker : svr_workers_g) {
    auto p = worker.site_info_->partition_id_;
    int sum = worker.DbChecksum();
    checksum_results[p].push_back(sum);
  }
  bool checksum_fail = false;
  for (auto& pair : checksum_results) {
    auto& vec = pair.second;
    for (auto checksum: vec) {
      if (checksum != vec[0]) {
        checksum_fail = true;
      }
    }
  }
  if (checksum_fail) {
    Log_warn("checksum match failed...perhaps wait longer before checksum?");
  }
#endif
#ifdef CPU_PROFILE
  // stop profiling
  ProfilerStop();
#endif // ifdef CPU_PROFILE
  client_shutdown();
  Log_info("After client_shutdown");
  
  for (int i = 0; i < 5; i++)
    cli2cli[6].merge(cli2cli[i]);
  Log_info("Fastpath count %d 0pct %.2f 50pct %.2f 90pct %.2f 99pct %.2f ave %.2f", cli2cli[0].count(), cli2cli[0].pct(0.0), cli2cli[0].pct50(), cli2cli[0].pct90(), cli2cli[0].pct99(), cli2cli[0].ave());
  Log_info("CoordinatorAccept count %d 0pct %.2f 50pct %.2f 90pct %.2f 99pct %.2f ave %.2f", cli2cli[1].count(), cli2cli[1].pct(0.0), cli2cli[1].pct50(), cli2cli[1].pct90(), cli2cli[1].pct99(), cli2cli[1].ave());
  Log_info("Fast-Original count %d 0pct %.2f 50pct %.2f 90pct %.2f 99pct %.2f ave %.2f", cli2cli[2].count(), cli2cli[2].pct(0.0), cli2cli[2].pct50(), cli2cli[2].pct90(), cli2cli[2].pct99(), cli2cli[2].ave());
  Log_info("Slow-Original count %d 0pct %.2f 50pct %.2f 90pct %.2f 99pct %.2f ave %.2f", cli2cli[3].count(), cli2cli[3].pct(0.0), cli2cli[3].pct50(), cli2cli[3].pct90(), cli2cli[3].pct99(), cli2cli[3].ave());
  Log_info("Original-Protocol count %d 0pct %.2f 50pct %.2f 90pct %.2f 99pct %.2f ave %.2f", cli2cli[4].count(), cli2cli[4].pct(0.0), cli2cli[4].pct50(), cli2cli[4].pct90(), cli2cli[4].pct99(), cli2cli[4].ave());
  Log_info("All original count %d 0pct %.2f 50pct %.2f 90pct %.2f 99pct %.2f ave %.2f", cli2cli[5].count(), cli2cli[5].pct(0.0), cli2cli[5].pct50(), cli2cli[5].pct90(), cli2cli[5].pct99(), cli2cli[5].ave());
  Log_info("Latency-50pct is %.2f ms, Latency-90pct is %.2f ms, Latency-99pct is %.2f ms, ave is %.2f ms", cli2cli[6].pct50(), cli2cli[6].pct90(), cli2cli[6].pct99(), cli2cli[6].ave());
  Log_info("Mid throughput is %.2f", cli2cli[6].count() / (Config::GetConfig()->duration_ / 3.0));
  Log_info("Original throughput is %.2f", cli2cli[5].count() / (Config::GetConfig()->duration_ / 3.0));
  Log_info("Fastpath statistics attempted %d successed %d rate(pct) %.2f", fastpath_attempted_count, fastpath_successed_count, fastpath_successed_count * 100.0 / fastpath_attempted_count);
  Log_info("Frequency: %s", frequency.top_keys_pcts().c_str());

  string dump_file_name = "results/recent_csv/" + Config::GetConfig()->exp_setting_name_ + ".csv";
  std::ofstream file(dump_file_name);
  if (!file.is_open()) {
    Log_info("Failed to open file for writing %s", dump_file_name.c_str());
  } else {
    file << "Fastpath" << "," << "CoordinatorAccept" << "," << "Fast-Original" << "," << "Slow-Original" << ","  << "Original-Protocol" << ","  << "All-Original" << "," << "Overall" << "," << "Commit-Time" << "\n";
    size_t max_size = commit_time.count();
    commit_time.pct50();
    for (int i = 0; i < 7; i++)
      if (cli2cli[i].count() > max_size)
        max_size = cli2cli[i].count();
    for (size_t i = 0; i < max_size; ++i) {
        for (int k = 0; k < 7; k++) {
          if (i < cli2cli[k].count())
            file << cli2cli[k].data_[i];
          file << ",";
        }
        file << commit_time.data_[i];
        file << "\n";
    }
    Log_info("Dumped to %s with %d lines data", dump_file_name.c_str(), max_size);
    file.close();
    Log_info("%s closed", dump_file_name.c_str());
  }
#ifdef LATENCY_DEBUG
  Log_info("client2leader 50pct %.2f 90pct %.2f 99pct %.2f", client2leader.pct50(), client2leader.pct90(), client2leader.pct99());
  Log_info("client2test_point 50pct %.2f 90pct %.2f 99pct %.2f", client2test_point.pct50(), client2test_point.pct90(), client2test_point.pct99());
  Log_info("client2leader_send 50pct %.2f 90pct %.2f 99pct %.2f", client2leader_send.pct50(), client2leader_send.pct90(), client2leader_send.pct99());
#endif
  // Log_info("FastPath-count = %d CoordinatorAccept-count = %d OriginalProtocol-count = %d", fastpath_count, coordinatoraccept_count, original_protocol_count);
  server_shutdown();
  // TODO, FIXME pending_future in rpc cause error.
  fflush(stderr);
  fflush(stdout);
  exit(0);
  return 0;
  
  Log_info("all server workers have shut down.");

  RandomGenerator::destroy();
  Config::DestroyConfig();

  Log_debug("exit process.");

  return 0;
}
