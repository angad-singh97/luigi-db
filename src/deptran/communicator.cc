
#include "communicator.h"
#include "coordinator.h"
#include "classic/coordinator.h"
#include "rcc/graph.h"
#include "rcc/graph_marshaler.h"
#include "command.h"
#include "command_marshaler.h"
#include "classic/tpc_command.h"
#include "procedure.h"
#include "rcc_rpc.h"
#include <typeinfo>
#include "RW_command.h"

namespace janus {

/************************RULE begin*********************************/

void RuleSpeculativeExecuteQuorumEvent::FeedResponse(bool y, value_t result, bool is_leader) {
  if (y) {
    if (has_result_) {
      verify(result == result_);
    } else {
      has_result_ = true;
      result_ = result;
    }
    if (is_leader)
      n_leader_yes_++;
    VoteYes();
  } else {
    if (is_leader)
      n_leader_no_++;
    VoteNo();
  }
}

bool RuleSpeculativeExecuteQuorumEvent::Yes() {
  return n_voted_yes_ >= quorum_ && n_leader_yes_ >= num_leader_;
}

bool RuleSpeculativeExecuteQuorumEvent::No() {
  // if ((n_voted_no_ > (n_total_ - quorum_)) || (n_leader_no_ > 0))
  //   Log_info("RuleSpeculativeExecuteQuorumEventNo: %d %d", n_voted_no_, n_leader_no_);
  return (n_voted_no_ > (n_total_ - quorum_)) || (n_leader_no_ > 0);
}

value_t RuleSpeculativeExecuteQuorumEvent::GetResult() {
  return result_;
}

/************************RULE end*********************************/

/************************CURP begin*********************************/

int CurpMaxFailure(int total) {
  return (total - 1) / 2;
}

int CurpFastQuorumSize(int total) {
  return CurpMaxFailure(total) + (CurpMaxFailure(total) + 1) / 2 + 1;
}

int CurpQuorumSize(int total) {
  return total - CurpMaxFailure(total);
}

int CurpSmallQuorumSize(int total) {
  return (CurpMaxFailure(total) + 1) / 2 + 1;
}

void CurpDispatchQuorumEvent::FeedResponse(bool_t accepted, ver_t ver, value_t result, int32_t finish_countdown, int32_t key_hotness, siteid_t coo_id) {
  // Log_info("[copilot+] CurpDispatchQuorumEvent FeedResponse accepted=%d i=%d j=%d ballot=%d", accepted, pos[0], pos[1], ballot);
  coo_id_vec_.push_back(coo_id);

  finish_countdown_ = max(finish_countdown_, finish_countdown);
  key_hotness_ = max(key_hotness_, key_hotness);
  if (accepted) {
    VoteYes();
    responses_.push_back(ResponsePack{ver, result});
  }
  else
    VoteNo();
}

bool CurpDispatchQuorumEvent::FastYes() {
  if (n_voted_yes_ < CurpFastQuorumSize(n_total_)) return false;
  // if (response_.size() == 0) return false;
  int max_len = FindMax();
  // Log_info("[copilot+] FastYes max_len=%d, CurpFastQuorumSize=%d judgement=%d", max_len, CurpFastQuorumSize(n_total_), max_len >= CurpFastQuorumSize(n_total_));
  // tmp1 = max_len;
  // tmp2 = CurpFastQuorumSize(n_total_);
  return max_len >= CurpFastQuorumSize(n_total_);
}

bool CurpDispatchQuorumEvent::FastNo() {
  // [CURP] TODO: rm below one line
  if (n_voted_yes_ + n_voted_no_ < CurpFastQuorumSize(n_total_)) return false;
  int max_len = FindMax();
  // Log_info("[copilot+] FastNo max_possible=%d, CurpFastQuorumSize=%d judgement=%d", max_len + (n_total_ - n_voted_yes_ - n_voted_no_), CurpFastQuorumSize(n_total_), max_len + (n_total_ - n_voted_yes_ - n_voted_no_) < CurpFastQuorumSize(n_total_));
  // tmp1 = max_len + (n_total_ - n_voted_yes_ - n_voted_no_);
  // tmp2 = CurpFastQuorumSize(n_total_);
  return max_len + (n_total_ - n_voted_yes_ - n_voted_no_) < CurpFastQuorumSize(n_total_);
}

bool CurpDispatchQuorumEvent::IsReady() {
  // Log_info("CurpDispatchQuorumEvent::IsReady");
  if (timeouted_) {
    // judgement_ = 0;
    // Log_info("[copilot+] timeouted_ ready");
    return true;
  }
  if (FastYes()) {
    // judgement_ = 1;
    // Log_info("[copilot+] FastYes ready");
    return true;
  } else if (FastNo()) {
    // judgement_ = 2;
    // Log_info("[copilot+] FastNo ready");
    return true;
  }
  return false;
}

siteid_t CurpDispatchQuorumEvent::GetCooId() {
  
  int max_len, max_value, cur_len;
  for (int i = 0; i < coo_id_vec_.size(); i++) {
    if (i == 0) {
      max_value = coo_id_vec_[i];
      max_len = cur_len = 1;
    } else if (coo_id_vec_[i] == coo_id_vec_[i - 1]) {
      if (++cur_len > max_len) {
        max_value = coo_id_vec_[i];
        max_len = cur_len;
      } else {
        cur_len = 1;
      }
    }
  }
  // [CURP] TODO: How much of max_len is enough?
  return max_value;
}


void CurpPrepareQuorumEvent::FeedResponse(bool y,
                                          int status,
                                          ballot_t last_accepted_ballot,
                                          MarshallDeputy md_cmd) {
  count_++;
  // max_seen_ballot_ = max(max_seen_ballot_, max_seen_ballot);
  shared_ptr<Marshallable> cmd = md_cmd.sp_data_;
  shared_ptr<SimpleRWCommand> parsed_cmd = make_shared<SimpleRWCommand>(cmd);
  if (status == CurpPlusData::CurpPlusStatus::COMMITTED || status == CurpPlusData::CurpPlusStatus::EXECUTED) {
    committed_cmd_ = cmd;
  }
  if (y) {
    pair<int, int> cmd_id = parsed_cmd->cmd_id_;
    if (status == CurpPlusData::CurpPlusStatus::ACCEPTED) {
      accepted_count_++;
      if (last_accepted_ballot > max_last_accepted_ballot_) {
        max_last_accepted_ballot_ = last_accepted_ballot;
        to_accept_cmd_ = cmd;
      }
    } else if (status == CurpPlusData::CurpPlusStatus::PREACCEPT) {
      fast_accept_[cmd_id].first++;
      fast_accept_[cmd_id].second = cmd;
      if (fast_accept_[cmd_id].first > max_fast_accept_count_) {
        max_fast_accept_count_ = fast_accept_[cmd_id].first;
#ifdef CURP_REPEAR_COMMIT_DEBUG
        Log_info("FeedResponse [%s] max_fast_accept_count_ updated to %d", parsed_cmd->cmd_to_string().c_str(), max_fast_accept_count_);
#endif
        max_fast_accept_id_ = cmd_id;
      }
    } else {
      // Do nothing
    }
    VoteYes();
  } else {
    VoteNo();
  }
}

bool CurpPrepareQuorumEvent::CommitYes() {
  return committed_cmd_ != nullptr;
}

bool CurpPrepareQuorumEvent::AcceptYes() {
  return Yes() && accepted_count_ > 0;
}

bool CurpPrepareQuorumEvent::FastAcceptYes() {
  return Yes() && max_fast_accept_count_ >= CurpSmallQuorumSize(n_total_);
}

bool CurpPrepareQuorumEvent::IsReady() {
  return timeouted_ || CommitYes() || AcceptYes() || FastAcceptYes() || Yes();
}

void CurpAcceptQuorumEvent::FeedResponse(bool y, ballot_t seen_ballot) {
  max_seen_ballot_ = max(max_seen_ballot_, seen_ballot);
  if (y)
    VoteYes();
  else
    VoteNo();
}


/************************CURP end*********************************/


uint64_t Communicator::global_id = 0;

Communicator::Communicator(PollMgr* poll_mgr) {
  vector<string> addrs;
  if (poll_mgr == nullptr)
    rpc_poll_ = new PollMgr(1);
  else
    rpc_poll_ = poll_mgr;
  auto config = Config::GetConfig();
  vector<parid_t> partitions = config->GetAllPartitionIds();
	Log_info("size of partitions: %d", partitions.size());
  for (auto& par_id : partitions) {
    auto site_infos = config->SitesByPartitionId(par_id);
    vector<std::pair<siteid_t, ClassicProxy*>> proxies;
    for (auto& si : site_infos) {
      auto result = ConnectToSite(si, std::chrono::milliseconds
          (CONNECT_TIMEOUT_MS));
      verify(result.first == SUCCESS);
      proxies.push_back(std::make_pair(si.id, result.second));
    }
    rpc_par_proxies_.insert(std::make_pair(par_id, proxies));
  }
  client_leaders_connected_.store(false);
  if (config->forwarding_enabled_) {
    threads.push_back(std::thread(&Communicator::ConnectClientLeaders, this));
  } else {
    client_leaders_connected_.store(true);
  }
}

void Communicator::ConnectClientLeaders() {
  auto config = Config::GetConfig();
  if (config->forwarding_enabled_) {
    Log_info("%s: connect to client sites", __FUNCTION__);
    auto client_leaders = config->SitesByLocaleId(0, Config::CLIENT);
    for (Config::SiteInfo leader_site_info : client_leaders) {
      verify(leader_site_info.locale_id == 0);
      Log_info("client @ leader %d", leader_site_info.id);
      auto result = ConnectToClientSite(leader_site_info,
                                        std::chrono::milliseconds
                                            (CONNECT_TIMEOUT_MS));
      verify(result.first == SUCCESS);
      verify(result.second != nullptr);
      Log_info("connected to client leader site: %d, %d, %p",
               leader_site_info.id,
               leader_site_info.locale_id,
               result.second);
      client_leaders_.push_back(std::make_pair(leader_site_info.id,
                                               result.second));
    }
  }
  client_leaders_connected_.store(true);
}

void Communicator::WaitConnectClientLeaders() {
  bool connected;
  do {
    connected = client_leaders_connected_.load();
  } while (!connected);
  Log_info("Done waiting to connect to client leaders.");
}

void Communicator::ResetProfiles(){
	index = 0;
	total = 0;
	for(int i = 0; i < 100; i++){
		window[i] = 0;
	}
	window_time = 0;
	total_time = 0;
	window_avg = 0;
	total_avg = 0;
}
Communicator::~Communicator() {
  verify(rpc_clients_.size() > 0);
  for (auto& pair : rpc_clients_) {
    auto rpc_cli = pair.second;
    rpc_cli->close_and_release();
  }
  rpc_clients_.clear();
}

std::pair<siteid_t, ClassicProxy*>
Communicator::RandomProxyForPartition(parid_t par_id) const {
  auto it = rpc_par_proxies_.find(par_id);
  verify(it != rpc_par_proxies_.end());
  auto& par_proxies = it->second;
  int index = rrr::RandomGenerator::rand(0, par_proxies.size() - 1);
  return par_proxies[index];
}

// for most protocol, e.g., Paxos or Raft, the client always 
//      tries to issue the request to the fixed leader (the first one) (idx is -1 by default)
// but, for Mencius, it uses round robin to rotate the leader (idx > -1)
// @param idx: get the index of servers as the leader
std::pair<siteid_t, ClassicProxy*>
Communicator::LeaderProxyForPartition(parid_t par_id, int idx) const {
  if (idx > -1) { // Mencius
    auto it = rpc_par_proxies_.find(par_id);
    auto& partition_proxies = it->second;
    verify(partition_proxies.size()>idx);
    return it->second.at(idx);
  }

  auto leader_cache =
      const_cast<map<parid_t, SiteProxyPair>&>(this->leader_cache_);
  auto leader_it = leader_cache.find(par_id);
  if (leader_it != leader_cache.end()) {
    return leader_it->second;
  } else {
    auto it = rpc_par_proxies_.find(par_id);
    verify(it != rpc_par_proxies_.end());
    auto& partition_proxies = it->second;
    auto config = Config::GetConfig();
    auto proxy_it = std::find_if(
        partition_proxies.begin(),
        partition_proxies.end(),
        [config](const std::pair<siteid_t, ClassicProxy*>& p) {
          verify(p.second != nullptr);
          auto& site = config->SiteById(p.first);
          return site.locale_id == 0;
        });
    if (proxy_it == partition_proxies.end()) {
      Log_fatal("could not find leader for partition %d", par_id);
    } else {
      leader_cache[par_id] = *proxy_it;
      Log_debug("leader site for parition %d is %d", par_id, proxy_it->first);
    }
    verify(proxy_it->second != nullptr);
    return *proxy_it;
  }
}

ClientSiteProxyPair
Communicator::ConnectToClientSite(Config::SiteInfo& site,
                                  std::chrono::milliseconds timeout) {
  auto config = Config::GetConfig();
  char addr[1024];
  snprintf(addr, sizeof(addr), "%s:%d", site.host.c_str(), site.port);

  auto start = std::chrono::steady_clock::now();
  rrr::Client* rpc_cli = new rrr::Client(rpc_poll_);
  double elapsed;
  int attempt = 0;
  do {
    Log_debug("connect to client site: %s (attempt %d)", addr, attempt++);
    auto connect_result = rpc_cli->connect(addr, false);
    if (connect_result == SUCCESS) {
      ClientControlProxy* rpc_proxy = new ClientControlProxy(rpc_cli);
      rpc_clients_.insert(std::make_pair(site.id, rpc_cli));
      Log_debug("connect to client site: %s success!", addr);
      return std::make_pair(SUCCESS, rpc_proxy);
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(CONNECT_SLEEP_MS));
    }
    auto end = std::chrono::steady_clock::now();
    elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
        .count();
  } while (elapsed < timeout.count());
  Log_info("timeout connecting to client %s", addr);
  rpc_cli->close_and_release();
  return std::make_pair(FAILURE, nullptr);
}

std::pair<int, ClassicProxy*>
Communicator::ConnectToSite(Config::SiteInfo& site,
                            std::chrono::milliseconds timeout) {
  string addr = site.GetHostAddr();
  auto start = std::chrono::steady_clock::now();
  auto rpc_cli = std::make_shared<rrr::Client>(rpc_poll_);
  double elapsed;
  int attempt = 0;
  do {
    Log_debug("connect to site: %s (attempt %d)", addr.c_str(), attempt++);
    auto connect_result = rpc_cli->connect(addr.c_str(), false);
    if (connect_result == SUCCESS) {
      ClassicProxy* rpc_proxy = new ClassicProxy(rpc_cli.get());
      rpc_clients_.insert(std::make_pair(site.id, rpc_cli));
      rpc_proxies_.insert(std::make_pair(site.id, rpc_proxy));

			auto it = Reactor::clients_.find(rpc_cli->host());
			if (it == Reactor::clients_.end()) {
				std::vector<std::shared_ptr<rrr::Pollable>> clients{};
				Reactor::clients_[rpc_cli->host()] = clients;
			}

			Reactor::clients_[rpc_cli->host()].push_back(rpc_cli);
      Log_info("connect to site: %s success!", addr.c_str());
      return std::make_pair(SUCCESS, rpc_proxy);
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(CONNECT_SLEEP_MS));
    }
    auto end = std::chrono::steady_clock::now();
    elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
        .count();
  } while (elapsed < timeout.count());
  Log_info("timeout connecting to %s", addr.c_str());
  rpc_cli->close_and_release();
  return std::make_pair(FAILURE, nullptr);
}

std::pair<siteid_t, ClassicProxy*>
Communicator::NearestProxyForPartition(parid_t par_id) const {
  // TODO Fix me.
  auto it = rpc_par_proxies_.find(par_id);
  verify(it != rpc_par_proxies_.end());
  auto& partition_proxies = it->second;
  verify(partition_proxies.size() > loc_id_);
  int index = loc_id_;
  return partition_proxies[index];
};

std::shared_ptr<QuorumEvent> Communicator::SendReelect(){
	//paused = true;
	//sleep(10);
	int total = rpc_par_proxies_[0].size() - 1;
  std::shared_ptr<QuorumEvent> e = Reactor::CreateSpEvent<QuorumEvent>(total, 1);
	auto pair_leader_proxy = LeaderProxyForPartition(0);
	int new_leader = (pair_leader_proxy.first + 1) % total;

	for(auto& pair: rpc_par_proxies_[0]){
		rrr::FutureAttr fuattr;
		int id = pair.first;
		if(id != 1) continue;
		fuattr.callback = 
			[e, this, id] (Future* fu) {
				bool_t success = false;
				fu->get_reply() >> success;
				
				if(success){
					e->VoteYes();
					this->SetNewLeaderProxy(0, id);
				}
			};
		for (int i = 0; i < 1000; i++) Log_info("sending reelect");
		auto f = pair.second->async_ReElect(fuattr);
		Future::safe_release(f);
	}
	return e;

}

void Communicator::BroadcastDispatch(
    shared_ptr<vector<shared_ptr<TxPieceData>>> sp_vec_piece,
    Coordinator* coo,
    const function<void(int, TxnOutput&)> & callback) {
  Log_debug("Do a dispatch on client worker");
  cmdid_t cmd_id = sp_vec_piece->at(0)->root_id_;
  verify(!sp_vec_piece->empty());
  auto par_id = sp_vec_piece->at(0)->PartitionId();

  rrr::FutureAttr fuattr;
  fuattr.callback =
      [coo, this, callback](Future* fu) {
        int32_t ret;
        TxnOutput outputs;
        fu->get_reply() >> ret >> outputs;
        callback(ret, outputs);
      };
  
  std::pair<siteid_t, ClassicProxy*> pair_leader_proxy;
  if (Config::GetConfig()->replica_proto_==MODE_MENCIUS || Config::GetConfig()->replica_proto_==MODE_MENCIUS_PLUS) {
    int n = rpc_par_proxies_.find(par_id)->second.size();
    pair_leader_proxy = LeaderProxyForPartition(par_id, coo->cli_id_% n);
  } else {
    pair_leader_proxy = LeaderProxyForPartition(par_id);
  }
  
  SetLeaderCache(par_id, pair_leader_proxy) ;
  Log_debug("send dispatch to site %ld, par %d",
            pair_leader_proxy.first, par_id);
  auto proxy = pair_leader_proxy.second;
  shared_ptr<VecPieceData> sp_vpd(new VecPieceData);
  sp_vpd->sp_vec_piece_data_ = sp_vec_piece;

  // Record Time
  sp_vpd->time_sent_from_client_ = SimpleRWCommand::GetCurrentMsTime();

  MarshallDeputy md(sp_vpd); // ????

	DepId di;
	di.str = "dep";
	di.id = Communicator::global_id++;
  
#ifdef CURP_TIME_DEBUG
  struct timeval tp;
  gettimeofday(&tp, NULL);
  Log_info("[CURP] [C-] BroadcastDispatch at Communicator %.3f", tp.tv_sec * 1000 + tp.tv_usec / 1000.0);
#endif

#ifdef COPILOT_TIME_DEBUG
  struct timeval tp;
  gettimeofday(&tp, NULL);
  Log_info("[CURP] [C-] BroadcastDispatch at Communicator %.3f", tp.tv_sec * 1000 + tp.tv_usec / 1000.0);
#endif

  WAN_WAIT;
#ifdef CURP_FULL_LOG_DEBUG
  Log_info("[CURP] cmd<%d, %d> before async_Dispatch", SimpleRWCommand::GetCmdID(md.sp_data_).first, SimpleRWCommand::GetCmdID(md.sp_data_).second);
#endif
	auto future = proxy->async_Dispatch(cmd_id, di, md, fuattr);
  Future::safe_release(future);
}


//need to change this code to solve the quorum info in the graphs
//either create another event here or inside the coordinator.
std::shared_ptr<IntEvent> Communicator::BroadcastDispatch(
    ReadyPiecesData cmds_by_par,
    Coordinator* coo,
    TxData* txn) {
  int total = cmds_by_par.size();
  //std::shared_ptr<AndEvent> e = Reactor::CreateSpEvent<AndEvent>();
  std::shared_ptr<IntEvent> e = Reactor::CreateSpEvent<IntEvent>();
	e->value_ = 0;
	e->target_ = total;
  std::unordered_set<int> leaders{};
  auto src_coroid = e->GetCoroId();
  coo->coro_id_ = src_coroid;
  Log_info("The size of cmds_by_par is %d", cmds_by_par.size());

  for(auto& pair: cmds_by_par){
    bool first = false;
    auto& cmds = pair.second;
    auto sp_vec_piece = std::make_shared<vector<shared_ptr<TxPieceData>>>();
    for(auto c: cmds){
      c->id_ = coo->next_pie_id();
      coo->dispatch_acks_[c->inn_id_] = false;
      sp_vec_piece->push_back(c);
    }
    cmdid_t cmd_id = sp_vec_piece->at(0)->root_id_;
    verify(sp_vec_piece->size() > 0);
    auto par_id = sp_vec_piece->at(0)->PartitionId();
    auto pair_leader_proxy = LeaderProxyForPartition(par_id);
    auto leader_id = pair_leader_proxy.first;

    phase_t phase = coo->phase_;
    rrr::FutureAttr fuattr;
    fuattr.callback =
        [e, coo, this, phase, txn, src_coroid, leader_id](Future* fu) {
          int32_t ret;
          TxnOutput outputs;
          uint64_t coro_id = 0;
	  			double cpu = 0.0;
	  			double net = 0.0;
          fu->get_reply() >> ret >> outputs >> coro_id;

          e->value_++;
          if(phase != coo->phase_){
						verify(0);
	    			e->Test();
	  			}
          else{
            if(ret == REJECT){
              coo->aborted_ = true;
              txn->commit_.store(false);

							e->value_ = e->target_;
							e->Test();
							return;
            }
            coo->n_dispatch_ack_ += outputs.size();
            for(auto& pair: outputs){
              const uint32_t& inn_id = pair.first;
              coo->dispatch_acks_[inn_id] = true;
              txn->Merge(pair.first, pair.second);
            }
	  
	    			CoordinatorClassic* classic_coo = (CoordinatorClassic*)coo;
	    			//classic_coo->debug_cnt--;
            if(txn->HasMoreUnsentPiece()){
              classic_coo->DispatchAsync(false);
            }
              //e->add_dep(coo->cli_id_, src_coroid, leader_id, coro_id);
            coo->ids_.push_back(leader_id);
            e->Test();
	  			}
      };
    
    Log_debug("send dispatch to site %ld",
              pair_leader_proxy.first);
    auto proxy = pair_leader_proxy.second;
    shared_ptr<VecPieceData> sp_vpd(new VecPieceData);
    sp_vpd->sp_vec_piece_data_ = sp_vec_piece;
    MarshallDeputy md(sp_vpd); // ????
    CoordinatorClassic* classic_coo = (CoordinatorClassic*) coo;
    //classic_coo->debug_cnt++;

    struct timespec start_;
    clock_gettime(CLOCK_REALTIME, &start_);

    outbound_[src_coroid] = make_pair((rrr::i64)start_.tv_sec, (rrr::i64)start_.tv_nsec);

		DepId di;
		di.str = "dep";
		di.id = Communicator::global_id++;
    
		auto future = proxy->async_Dispatch(cmd_id, di, md, fuattr);
    Future::safe_release(future);
    if(!broadcasting_to_leaders_only_){
      for (auto& pair : rpc_par_proxies_[par_id]) {
        if (pair.first != pair_leader_proxy.first) {
          //if(first) curr->n_total_++;
          auto follower_id = pair.first;
          rrr::FutureAttr fuattr;
          fuattr.callback =
              [e, coo, this, src_coroid, follower_id](Future* fu) {
                int32_t ret;
                TxnOutput outputs;
                uint64_t coro_id = 0;
                fu->get_reply() >> ret >> outputs >> coro_id;
                //e->add_dep(coo->cli_id_, src_coroid, follower_id, coro_id);
                //coo->ids_.push_back(follower_id);
                // do nothing
              };
					DepId di2;
					di2.str = "dep";
					di2.id = Communicator::global_id++;
          
					Future::safe_release(pair.second->async_Dispatch(cmd_id, di2, md, fuattr));
        }
      }
    }
  }
  //probably should modify the data structure here.
  return e;
}


void Communicator::SendStart(SimpleCommand& cmd,
                             int32_t output_size,
                             std::function<void(Future* fu)>& callback) {
  verify(0);
}

shared_ptr<AndEvent>
Communicator::SendPrepare(Coordinator* coo,
                          txnid_t tid,
                          std::vector<int32_t>& sids){
	int32_t res_ = 10;
  TxData* cmd = (TxData*) coo->cmd_;
  auto n = cmd->partition_ids_.size();
  auto e = Reactor::CreateSpEvent<AndEvent>();
  auto phase = coo->phase_;
  int n_total = 1;
  int quorum_id = 0;
  for(auto& partition_id : cmd->partition_ids_){
    auto leader_id = LeaderProxyForPartition(partition_id).first;
    auto site_id = leader_id;
    auto proxies = rpc_par_proxies_[partition_id];
    if(follower_forwarding) n_total = 3;
    auto qe = Reactor::CreateSpEvent<QuorumEvent>(n_total, 1);
    e->AddEvent(qe);
    auto src_coroid = qe->GetCoroId();
      
    qe->id_ = Communicator::global_id;
    qe->par_id_ = quorum_id++;
    FutureAttr fuattr;
    fuattr.callback = [this, e, qe, src_coroid, site_id, coo, phase, cmd](Future* fu) {
      int32_t res;
			bool_t slow;
      uint64_t coro_id = 0;
      fu->get_reply() >> res >> slow >>coro_id;
			
			this->slow = slow;
      // qe->add_dep(coo->cli_id_, src_coroid, site_id, coro_id); 
      
      if(phase != coo->phase_){
        return;
      }

      if(res == REJECT){
				Log_info("REJECT in prepare");
        cmd->commit_.store(false);
        coo->aborted_ = true;
      }
      qe->n_voted_yes_++;
      e->Test();
    };
    
    ClassicProxy* proxy = LeaderProxyForPartition(partition_id).second;
    Log_debug("SendPrepare to %ld sites gid:%ld, tid:%ld\n",
              sids.size(),
              partition_id,
              tid);
		DepId di;
		di.str = "dep";
		di.id = Communicator::global_id++;
    
		Future::safe_release(proxy->async_Prepare(tid, sids, di, fuattr));
    if(follower_forwarding){
      for(auto& pair : rpc_par_proxies_[partition_id]){
        if(pair.first != leader_id){
          site_id = pair.first;
          proxy = pair.second;
					
					DepId di2;
					di2.str = "dep";
					di2.id = Communicator::global_id++;
          
					Future::safe_release(proxy->async_Prepare(tid, sids, di2, fuattr));  
        }
      }
    }
  }
  return e;
}

/*void Communicator::SendPrepare(groupid_t gid,
                               txnid_t tid,
                               std::vector<int32_t>& sids,
                               const function<void(int)>& callback) {
  FutureAttr fuattr;
  std::function<void(Future*)> cb =
      [this, callback](Future* fu) {
        int res;
        fu->get_reply() >> res;
        callback(res);
      };
  fuattr.callback = cb;
  // ClassicProxy* proxy = LeaderProxyForPartition(gid).second;
  auto pair_proxies = PilotProxyForPartition(gid);
  verify(pair_proxies.size() == 2);
  Log_debug("SendPrepare to %ld sites gid:%ld, tid:%ld\n",
            sids.size(),
            gid,
            tid);
  for (auto& p : pair_proxies)
    Future::safe_release(p.second->async_Prepare(tid, sids, fuattr));
}*/

void Communicator::___LogSent(parid_t pid, txnid_t tid) {
  auto value = std::make_pair(pid, tid);
  auto it = phase_three_sent_.find(value);
  if (it != phase_three_sent_.end()) {
    Log_fatal("phase 3 sent exists: %d %x", it->first, it->second);
  } else {
    phase_three_sent_.insert(value);
    Log_debug("phase 3 sent: pid: %d; tid: %x", value.first, value.second);
  }
}

shared_ptr<AndEvent>
Communicator::SendCommit(Coordinator* coo,
                              txnid_t tid) {
#ifdef LOG_LEVEL_AS_DEBUG
//  ___LogSent(pid, tid);
#endif
	TxData* cmd = (TxData*) coo->cmd_;
  int n_total = 1;
  auto n = cmd->GetPartitionIds().size();
  auto e = Reactor::CreateSpEvent<AndEvent>();
  
  for(auto& rp : cmd->partition_ids_){
    auto leader_id = LeaderProxyForPartition(rp).first;
    auto site_id = leader_id;
    auto proxies = rpc_par_proxies_[rp];
    if(follower_forwarding) n_total = 3;
    auto qe = Reactor::CreateSpEvent<QuorumEvent>(n_total, 1);
    qe->id_ = Communicator::global_id;
    auto src_coroid = qe->GetCoroId();

    e->AddEvent(qe);

    coo->n_finish_req_++;
    FutureAttr fuattr;
    auto phase = coo->phase_;
    fuattr.callback = [this, e, qe, src_coroid, site_id, coo, phase, cmd](Future* fu) {
      int32_t res;
			bool_t slow;
      uint64_t coro_id = 0;
			Profiling profile;
      fu->get_reply() >> res >> slow >> coro_id >> profile;
			this->slow = slow;
			if(profile.cpu_util >= 0.0){
				cpu = profile.cpu_util;
				//Log_info("cpu: %f and network: %f and memory: %f", profile.cpu_util, profile.tx_util, profile.mem_util);
			}

      struct timespec end_;
	  	clock_gettime(CLOCK_REALTIME,&end_);

	  	rrr::i64 start_sec = this->outbound_[src_coroid].first;
	  	rrr::i64 start_nsec = this->outbound_[src_coroid].second;

	  	rrr::i64 curr = ((rrr::i64)end_.tv_sec - start_sec)*1000000000 + ((rrr::i64)end_.tv_nsec - start_nsec);
	  	curr /= 1000;
	  	this->total_time += curr;
	  	this->total++;
      if(this->index < 200){
	    	this->window[this->index] = curr;
	    	this->index++;
	    	this->window_time = this->total_time;
	  	}
      else{
	    	this->window_time = 0;
	    	for(int i = 0; i < 199; i++){
	      	this->window[i] = this->window[i+1];
	      	this->window_time += this->window[i];
	    	}
	    	this->window[199] = curr;
	    	this->window_time += curr;
	  	}
			this->window_avg = this->window_time/this->index;
			this->total_avg = this->total_time/this->total;

      // qe->add_dep(coo->cli_id_, src_coroid, site_id, coro_id);

      if(coo->phase_ != phase) return;
      qe->n_voted_yes_++;
      e->Test();
    };

		DepId di;
		di.str = "dep";
		di.id = Communicator::global_id++;
    ClassicProxy* proxy = LeaderProxyForPartition(rp).second;
    Log_debug("SendCommit to %ld tid:%ld\n", rp, tid);
    Future::safe_release(proxy->async_Commit(tid, di, fuattr));
    
    if(follower_forwarding){
      for(auto& pair : rpc_par_proxies_[rp]){
        if(pair.first != leader_id){
					DepId di2;
					di2.str = "dep";
					di2.id = Communicator::global_id++;
          
					site_id = pair.first;
          proxy = pair.second;
          Future::safe_release(proxy->async_Commit(tid, di2, fuattr));  
        }
      }
    }

    coo->site_commit_[rp]++;

  }
  return e;
}

/*void Communicator::SendCommit(parid_t pid,
                              txnid_t tid,
                              const function<void()>& callback) {
#ifdef LOG_LEVEL_AS_DEBUG
  ___LogSent(pid, tid);
#endif
  FutureAttr fuattr;
  fuattr.callback = [callback](Future*) { callback(); };
  auto proxy_pair = LeaderProxyForPartition(pid);
  ClassicProxy* proxy = proxy_pair.second;
  SetLeaderCache(pid, proxy_pair) ;
  Log_debug("SendCommit to %ld tid:%ld\n", pid, tid);
  Future::safe_release(proxy->async_Commit(tid, 0, fuattr));
}*/
shared_ptr<AndEvent>
Communicator::SendAbort(Coordinator* coo,
                              txnid_t tid) {
#ifdef LOG_LEVEL_AS_DEBUG
//  ___LogSent(pid, tid);
#endif
	TxData* cmd = (TxData*) coo->cmd_;
  int n_total = 1;
  auto n = cmd->GetPartitionIds().size();
  auto e = Reactor::CreateSpEvent<AndEvent>();
  for(auto& rp : cmd->partition_ids_){
    auto proxies = rpc_par_proxies_[rp];
    auto leader_id = LeaderProxyForPartition(rp).first;
    auto site_id = leader_id;
    if(follower_forwarding) n_total = 3;
    auto qe = Reactor::CreateSpEvent<QuorumEvent>(n_total, 1);
    qe->id_ = Communicator::global_id;
    auto src_coroid = qe->GetCoroId();

    e->AddEvent(qe);

    coo->n_finish_req_++;
    FutureAttr fuattr;
    auto phase = coo->phase_;
    fuattr.callback = [this, e, qe, coo, src_coroid, site_id, phase, cmd](Future* fu) {
      int32_t res;
			bool_t slow;
      uint64_t coro_id = 0;
			Profiling profile;
      fu->get_reply() >> res >> slow >> coro_id >> profile;
			this->slow = slow;

	  	if(profile.cpu_util != -1.0){
				Log_info("cpu: %f and network: %f", profile.cpu_util, profile.tx_util);
				this->cpu = profile.cpu_util;
				this->tx = profile.tx_util;
			}

      struct timespec end_;
	  	clock_gettime(CLOCK_REALTIME,&end_);

	  	rrr::i64 start_sec = this->outbound_[src_coroid].first;
	  	rrr::i64 start_nsec = this->outbound_[src_coroid].second;

	  	rrr::i64 curr = ((rrr::i64)end_.tv_sec - start_sec)*1000000000 + ((rrr::i64)end_.tv_nsec - start_nsec);
	  	curr /= 1000;
	  	this->total_time += curr;
	  	this->total++;
      if(this->index < 100){
	    	this->window[this->index];
	    	this->index++;
	    	this->window_time = this->total_time;
	  	}
      else{
	    	this->window_time = 0;
	    	for(int i = 0; i < 99; i++){
	      	this->window[i] = this->window[i+1];
	      	this->window_time += this->window[i];
	    	}
	    	this->window[99] = curr;
	    	this->window_time += curr;
	  	}
	  	//Log_info("average time of RPC is: %d", this->total_time/this->total);
	 		//Log_info("window time of RPC is: %d", this->window_time/this->index);

      // qe->add_dep(coo->cli_id_, src_coroid, site_id, coro_id); 

      if(coo->phase_ != phase) return;
      qe->n_voted_yes_++;
      e->Test();
    };

		DepId di;
		di.str = "dep";
		di.id = Communicator::global_id++;
    ClassicProxy* proxy = LeaderProxyForPartition(rp).second;
    Log_debug("SendAbort to %ld tid:%ld\n", rp, tid);
    Future::safe_release(proxy->async_Abort(tid, di, fuattr));

    if(follower_forwarding){
      for(auto& pair : rpc_par_proxies_[rp]){
        if(pair.first != leader_id){
					DepId di2;
					di2.str = "dep";
					di2.id = Communicator::global_id++;
          
					site_id = pair.first;
          proxy = pair.second;
          Future::safe_release(proxy->async_Abort(tid, di2, fuattr));  
        }
      }

    }
    coo->site_abort_[rp]++;
  }
  return e;
}

void Communicator::SendEarlyAbort(parid_t pid,
                                  txnid_t tid) {
#ifdef LOG_LEVEL_AS_DEBUG
  ___LogSent(pid, tid);
#endif
  FutureAttr fuattr;
  fuattr.callback = [](Future*) {};
  ClassicProxy* proxy = LeaderProxyForPartition(pid).second;
  Log_debug("SendAbort to %ld tid:%ld\n", pid, tid);
  Future::safe_release(proxy->async_EarlyAbort(tid, fuattr));
}

/*void Communicator::SendAbort(parid_t pid, txnid_t tid,
                             const function<void()>& callback) {
#ifdef LOG_LEVEL_AS_DEBUG
  ___LogSent(pid, tid);
#endif
  FutureAttr fuattr;
  fuattr.callback = [callback](Future*) { callback(); };
  // ClassicProxy* proxy = LeaderProxyForPartition(pid).second;
  auto pair_proxies = PilotProxyForPartition(pid);
  Log_debug("SendAbort to %ld tid:%ld\n", pid, tid);
  for (auto& p : pair_proxies)
    Future::safe_release(p.second->async_Abort(tid, fuattr));
}*/

void Communicator::SendUpgradeEpoch(epoch_t curr_epoch,
                                    const function<void(parid_t,
                                                        siteid_t,
                                                        int32_t&)>& callback) {
  for (auto& pair: rpc_par_proxies_) {
    auto& par_id = pair.first;
    auto& proxies = pair.second;
    for (auto& pair: proxies) {
      FutureAttr fuattr;
      auto& site_id = pair.first;
      function<void(Future*)> cb = [callback, par_id, site_id](Future* fu) {
        int32_t res;
        fu->get_reply() >> res;
        callback(par_id, site_id, res);
      };
      fuattr.callback = cb;
      auto proxy = (ClassicProxy*) pair.second;
      Future::safe_release(proxy->async_UpgradeEpoch(curr_epoch, fuattr));
    }
  }
}

void Communicator::SendTruncateEpoch(epoch_t old_epoch) {
  for (auto& pair: rpc_par_proxies_) {
    auto& par_id = pair.first;
    auto& proxies = pair.second;
    for (auto& pair: proxies) {
      FutureAttr fuattr;
      fuattr.callback = [](Future*) {};
      auto proxy = (ClassicProxy*) pair.second;
      Future::safe_release(proxy->async_TruncateEpoch(old_epoch));
    }
  }
}

void Communicator::SendForwardTxnRequest(
    TxRequest& req,
    Coordinator* coo,
    std::function<void(const TxReply&)> callback) {
  Log_info("%s: %d, %d", __FUNCTION__, coo->coo_id_, coo->par_id_);
  verify(client_leaders_.size() > 0);
  auto idx = rrr::RandomGenerator::rand(0, client_leaders_.size() - 1);
  auto p = client_leaders_[idx];
  auto leader_site_id = p.first;
  auto leader_proxy = p.second;
  Log_debug("%s: send to client site %d", __FUNCTION__, leader_site_id);
  TxDispatchRequest dispatch_request;
  dispatch_request.id = coo->coo_id_;
  for (size_t i = 0; i < req.input_.size(); i++) {
    dispatch_request.input.push_back(req.input_[i]);
  }
  dispatch_request.tx_type = req.tx_type_;

  FutureAttr future;
  future.callback = [callback](Future* f) {
    TxReply reply;
    f->get_reply() >> reply;
    callback(reply);
  };
  Future::safe_release(leader_proxy->async_DispatchTxn(dispatch_request,
                                                       future));
}

vector<shared_ptr<MessageEvent>>
Communicator::BroadcastMessage(shardid_t shard_id,
                               svrid_t svr_id,
                               string& msg) {
  verify(0);
  // TODO
  vector<shared_ptr<MessageEvent>> events;

  for (auto& p : rpc_par_proxies_[shard_id]) {
    auto site_id = p.first;
    auto proxy = (p.second);
    verify(proxy != nullptr);
    FutureAttr fuattr;
    auto msg_ev = std::make_shared<MessageEvent>(shard_id, site_id);
    events.push_back(msg_ev);
    fuattr.callback = [msg_ev] (Future* fu) {
      auto& marshal = fu->get_reply();
      marshal >> msg_ev->msg_;
      msg_ev->Set(1);
    };
    Future* f = nullptr;
    Future::safe_release(f);
    return events;
  }
}

shared_ptr<MessageEvent>
Communicator::SendMessage(siteid_t site_id,
                          string& msg) {
  verify(0);
  // TODO
  auto ev = std::make_shared<MessageEvent>(site_id);
  return ev;
}


void Communicator::AddMessageHandler(
    function<bool(const MarshallDeputy&, MarshallDeputy&)> f) {
   msg_marshall_handlers_.push_back(f);
}

shared_ptr<GetLeaderQuorumEvent> Communicator::BroadcastGetLeader(
    parid_t par_id, locid_t cur_pause) {
  int n = Config::GetConfig()->GetPartitionSize(par_id);
  auto e = Reactor::CreateSpEvent<GetLeaderQuorumEvent>(n - 1, 1);
  auto proxies = rpc_par_proxies_[par_id];
  WAN_WAIT;
  for (auto& p : proxies) {
    if (p.first == cur_pause) continue;
    auto proxy = p.second;
    FutureAttr fuattr;
    fuattr.callback = [e, p](Future* fu) {
      bool_t is_leader = false;
      fu->get_reply() >> is_leader;
      e->FeedResponse(is_leader, p.first);
    };
    Future::safe_release(proxy->async_IsFPGALeader(par_id, fuattr));
  }
  return e;
}

shared_ptr<QuorumEvent> Communicator::SendFailOverTrig(
    parid_t par_id, locid_t loc_id, bool pause) {
  int n = Config::GetConfig()->GetPartitionSize(par_id);
  auto e = Reactor::CreateSpEvent<QuorumEvent>(1, 1);
  auto proxies = rpc_par_proxies_[par_id];
  WAN_WAIT;
  for (auto& p : proxies) {
    if (p.first != loc_id) continue;
    auto proxy = p.second;
    FutureAttr fuattr;
    fuattr.callback = [e](Future* fu) {
      int res;
      fu->get_reply() >> res;
      if (res == 0)
        e->VoteYes();
      else
        e->VoteNo();
    };
    Future::safe_release(proxy->async_FailOverTrig(pause, fuattr));
  }
  return e;
}

void Communicator::SetNewLeaderProxy(parid_t par_id, locid_t loc_id) {
  bool found = false;
  auto proxies = rpc_par_proxies_[par_id];
  for (auto& p : proxies) {
    if (p.first == loc_id) {
      leader_cache_[par_id] = p;
      found = true;
      break;
    }
  }

  verify(found);

  /*  auto it = rpc_par_proxies_.find(par_id);
    verify(it != rpc_par_proxies_.end());
    auto& partition_proxies = it->second;
    auto config = Config::GetConfig();
    auto proxy_it = std::find_if(
        partition_proxies.begin(),
        partition_proxies.end(),
        [config, loc_id](const std::pair<siteid_t, ClassicProxy*>& p) {
          verify(p.second != nullptr);
          auto& site = config->SiteById(p.first);
          return site.locale_id == loc_id ;
        });
     verify (proxy_it != partition_proxies.end()) ;
     leader_cache_[par_id] = *proxy_it;*/
  Log_debug("set leader porxy for parition %d is %d", par_id, loc_id);
}

void Communicator::SendSimpleCmd(groupid_t gid, SimpleCommand& cmd,
    std::vector<int32_t>& sids, const function<void(int)>& callback) {
  FutureAttr fuattr;
  std::function<void(Future*)> cb = [this, callback](Future* fu) {
    int res;
    fu->get_reply() >> res;
    callback(res);
  };
  fuattr.callback = cb;
  ClassicProxy* proxy = LeaderProxyForPartition(gid).second;
  Log_debug("SendEmptyCmd to %ld sites gid:%ld\n", sids.size(), gid);
  Future::safe_release(proxy->async_SimpleCmd(cmd, fuattr));
}

// below are about CURP

// [CURP] TODO: Haven't consider partition
shared_ptr<CurpDispatchQuorumEvent>
Communicator::CurpBroadcastDispatch(shared_ptr<Marshallable> cmd) {
  // shared_ptr<TpcCommitCommand> tpc_cmd = dynamic_pointer_cast<TpcCommitCommand>(cmd);
  // VecPieceData *cmd_cast = (VecPieceData*)(tpc_cmd->cmd_.get());
  shared_ptr<vector<shared_ptr<TxPieceData>>> sp_vec_piece = dynamic_pointer_cast<VecPieceData>(cmd)->sp_vec_piece_data_;
  verify(!sp_vec_piece->empty());
  auto par_id = sp_vec_piece->at(0)->PartitionId();
  
  MarshallDeputy md(cmd);

  int n = Config::GetConfig()->GetPartitionSize(par_id);
  auto e = Reactor::CreateSpEvent<CurpDispatchQuorumEvent>(n, CurpQuorumSize(n));
  WAN_WAIT;
  for (auto& pair : rpc_par_proxies_[par_id]) {
    rrr::FutureAttr fuattr;
    fuattr.callback =
        [e, this](Future* fu) {
          bool_t accepted;
          ver_t ver;
          value_t result;
          int32_t finish_countdown;
          int32_t key_hotness;
          siteid_t coo_id;
          fu->get_reply() >> accepted >> ver >> result >> finish_countdown >> key_hotness >> coo_id;
          e->FeedResponse(accepted, ver, result, finish_countdown, key_hotness, coo_id);
        };
    
    DepId di;
    di.str = "dep";
    di.id = Communicator::global_id++;
    
    auto proxy = (CurpProxy *)pair.second;

#ifdef CURP_TIME_DEBUG
    struct timeval tp;
    gettimeofday(&tp, NULL);
    Log_info("[CURP] [1-] [tx=%d] async_PoorDispatch called by Submit %.3f", tpc_cmd->tx_id_, tp.tv_sec * 1000 + tp.tv_usec / 1000.0);
#endif
    // Record Time
    struct timeval tp;
    gettimeofday(&tp, NULL);
    dynamic_pointer_cast<VecPieceData>(cmd)->time_sent_from_client_ = tp.tv_sec * 1000 + tp.tv_usec / 1000.0;
    
    // Log_info("[CURP] async_CurpPoorDispatch of cmd<%d, %d>", sp_vec_piece->at(0)->client_id_, sp_vec_piece->at(0)->cmd_id_in_client_);
    auto future = proxy->async_CurpDispatch(sp_vec_piece->at(0)->client_id_, sp_vec_piece->at(0)->cmd_id_in_client_, md, fuattr);
    Future::safe_release(future);
  }

  e->Wait();

  return e;
}

// shared_ptr<IntEvent>
// Communicator::OriginalDispatch(shared_ptr<Marshallable> cmd, siteid_t target_site, i64 dep_id) {
//   shared_ptr<TpcCommitCommand> tpc_cmd = dynamic_pointer_cast<TpcCommitCommand>(cmd);
//   VecPieceData *cmd_cast = (VecPieceData*)(tpc_cmd->cmd_.get());
//   shared_ptr<vector<shared_ptr<TxPieceData>>> sp_vec_piece = cmd_cast->sp_vec_piece_data_;
//   verify(!sp_vec_piece->empty());
//   auto par_id = sp_vec_piece->at(0)->PartitionId();
  
//   shared_ptr<VecPieceData> sp_vpd(new VecPieceData);
//   sp_vpd->sp_vec_piece_data_ = sp_vec_piece;
//   MarshallDeputy md(cmd);

//   auto e = Reactor::CreateSpEvent<IntEvent>();
//   for (auto& pair : rpc_par_proxies_[par_id]) 
//     if (pair.first == target_site) {
//       rrr::FutureAttr fuattr;
//       fuattr.callback =
//           [e, this](Future* fu) {
//             bool_t slow;
//             fu->get_reply() >> slow;
//             this->slow = slow;
//             e->Set(1);
//           };
      
//       auto proxy = (CurpProxy *)pair.second;

//       auto future = proxy->async_OriginalSubmit(md, dep_id, fuattr);
//       Future::safe_release(future);
//     }

//   e->Wait();

//   return e;
// }

shared_ptr<CurpWaitCommitQuorumEvent>
Communicator::CurpBroadcastWaitCommit(shared_ptr<Marshallable> cmd,
                                            siteid_t coo_id) {
  // shared_ptr<TpcCommitCommand> tpc_cmd = dynamic_pointer_cast<TpcCommitCommand>(cmd);
  // VecPieceData *cmd_cast = (VecPieceData*)(tpc_cmd->cmd_.get());
  shared_ptr<VecPieceData> cmd_cast = dynamic_pointer_cast<VecPieceData>(cmd);
  shared_ptr<vector<shared_ptr<TxPieceData>>> vec_piece_data = cmd_cast->sp_vec_piece_data_;

  int32_t client_id_ = vec_piece_data->at(0)->client_id_;
  int32_t cmd_id_in_client_ = vec_piece_data->at(0)->cmd_id_in_client_;
  auto par_id = vec_piece_data->at(0)->PartitionId();
  int n = Config::GetConfig()->GetPartitionSize(par_id);
  auto e = Reactor::CreateSpEvent<CurpWaitCommitQuorumEvent>();

  shared_ptr<VecPieceData> sp_vpd(new VecPieceData);
  // sp_vpd->sp_vec_piece_data_ = sp_vec_piece;
  sp_vpd->sp_vec_piece_data_ = vec_piece_data;
  MarshallDeputy md(sp_vpd);
  WAN_WAIT;
  for (auto& pair : rpc_par_proxies_[par_id])
    if (pair.first == coo_id) {
      rrr::FutureAttr fuattr;
      fuattr.callback =
          [this, e](Future* fu) {
            bool_t committed;
            value_t commit_result;
            fu->get_reply() >> committed >> commit_result;
#ifdef CURP_FULL_LOG_DEBUG
            Log_info("[CURP] loc=%d CurpBroadcastWaitCommit reply committed=%d commit_result=%d", loc_id_, committed, commit_result);
#endif
            e->FeedResponse(committed, commit_result);
          };
      auto proxy = (CurpProxy *)pair.second;
      auto future = proxy->async_CurpWaitCommit(client_id_, cmd_id_in_client_, fuattr);
      Future::safe_release(future);
  }
  return e;
}

shared_ptr<IntEvent>
Communicator::CurpForwardResultToCoordinator(parid_t par_id,
                                              bool_t accepted,
                                              ver_t ver,
                                              const shared_ptr<Marshallable>& cmd) {
  int n = Config::GetConfig()->GetPartitionSize(par_id);
  auto e = Reactor::CreateSpEvent<IntEvent>();
  auto proxies = rpc_par_proxies_[par_id];
  MarshallDeputy cmd_deputy(cmd);
  WAN_WAIT;
  for (auto& p : proxies) {
    auto proxy = (CurpProxy *)p.second;
    auto site = p.first;
    // TODO: generelize
    if (0 == site) {
      FutureAttr fuattr;
      fuattr.callback = [](Future *fu) {};

#ifdef CURP_TIME_DEBUG
      struct timeval tp;
      gettimeofday(&tp, NULL);
      Log_info("[CURP] [2-] [tx=%d] Forward %.3f", dynamic_pointer_cast<TpcCommitCommand>(cmd)->tx_id_, tp.tv_sec * 1000 + tp.tv_usec / 1000.0);
#endif

      Future *f = proxy->async_CurpForward(accepted, ver, cmd_deputy, fuattr);
      Future::safe_release(f);
    }
  }
  return e;
}

shared_ptr<CurpPrepareQuorumEvent>
Communicator::CurpBroadcastPrepare(parid_t par_id,
                  key_t key,
                  ver_t ver,
                  ballot_t ballot,
                  uint32_t self_loc) {
  int n = Config::GetConfig()->GetPartitionSize(par_id);
  auto e = Reactor::CreateSpEvent<CurpPrepareQuorumEvent>(n, ballot);
  auto proxies = rpc_par_proxies_[par_id];
  WAN_WAIT;
  for (auto& p : proxies) {
    auto proxy = (CurpProxy *)p.second;
    auto site = p.first;
    FutureAttr fuattr;
    fuattr.callback = [e, site, self_loc](Future *fu) {
      bool_t accepted;
      i32 status;
      ballot_t last_accepted_ballot;
      MarshallDeputy cmd;
      fu->get_reply() >> accepted >> status >> last_accepted_ballot >> cmd;
      accepted = accepted || (site == self_loc);
      e->FeedResponse(accepted, status, last_accepted_ballot, cmd);
    };
    // Log_info("[CURP] async_CurpPrepare(%d, %d, %d)", key, ver, ballot);
    Future *f = proxy->async_CurpPrepare(key, ver, ballot, fuattr);
    Future::safe_release(f);
  }
  return e;
}

shared_ptr<CurpAcceptQuorumEvent>
Communicator::CurpBroadcastAccept(parid_t par_id,
                                  ver_t ver,
                                  ballot_t ballot,
                                  shared_ptr<Marshallable> cmd) {
  int n = Config::GetConfig()->GetPartitionSize(par_id);
  auto e = Reactor::CreateSpEvent<CurpAcceptQuorumEvent>(n);
  auto proxies = rpc_par_proxies_[par_id];
  MarshallDeputy cmd_deputy(cmd);
  WAN_WAIT;
  for (auto& p : proxies) {
    auto proxy = (CurpProxy *)p.second;
    auto site = p.first;
    FutureAttr fuattr;
    fuattr.callback = [e](Future *fu) {
      bool_t accepted;
      ballot_t seen_ballot;
      fu->get_reply() >> accepted >> seen_ballot;
      e->FeedResponse(accepted, seen_ballot);
    };
    Future *f = proxy->async_CurpAccept(ver, ballot, cmd_deputy, fuattr);
    Future::safe_release(f);
  }
  return e;
}

shared_ptr<IntEvent>
Communicator::CurpBroadcastCommit(parid_t par_id,
                                ver_t ver,
                                shared_ptr<Marshallable> cmd,
                                uint16_t ban_site) {
  int n = Config::GetConfig()->GetPartitionSize(par_id);
  auto e = Reactor::CreateSpEvent<IntEvent>();
  auto proxies = rpc_par_proxies_[par_id];
  MarshallDeputy cmd_deputy(cmd);
  WAN_WAIT;
  for (auto& p : proxies) {
    auto proxy = (CurpProxy *)p.second;
    auto site = p.first;
    if (site != ban_site) {
      FutureAttr fuattr;
      fuattr.callback = [](Future *fu) {};
      // Log_info("[CURP] Broadcast Commit to site %d", site);
      Future *f = proxy->async_CurpCommit(ver, cmd_deputy, fuattr);
      Future::safe_release(f);
    }
  }
  return e;
}

void Communicator::RuleBroadcastWitnessGC(parid_t par_id,
                                          shared_ptr<Marshallable> cmd,
                                          uint16_t ban_site) {
  int n = Config::GetConfig()->GetPartitionSize(par_id);
  auto proxies = rpc_par_proxies_[par_id];
  MarshallDeputy cmd_deputy(cmd);
  WAN_WAIT;
  for (auto& p : proxies) {
    auto proxy = p.second;
    auto site = p.first;
    if (site != ban_site) {
      FutureAttr fuattr;
      fuattr.callback = [](Future *fu) {};
      // Log_info("[CURP] Broadcast Commit to site %d", site);
      Future *f = proxy->async_RuleWitnessGC(cmd_deputy, fuattr);
      Future::safe_release(f);
    }
  }
}

} // namespace janus
