#include "commo.h"

namespace janus
{

SiteProxyPair CommunicatorRule::FindSiteProxyPair(parid_t par_id, int replica_id) const {
  auto it  = rpc_par_proxies_.find(par_id);
  verify(it != rpc_par_proxies_.end());
  auto& partition_proxies = it->second;
  auto config = Config::GetConfig();
  auto proxy_pair =
      std::find_if(partition_proxies.begin(), partition_proxies.end(),
                   [config, replica_id](const std::pair<siteid_t, ClassicProxy*>& p) {
                     verify(p.second != nullptr);
                     auto& site = config->SiteById(p.first);
                     return site.locale_id == replica_id;
                   });
  if (proxy_pair == partition_proxies.end())
    Log_fatal("couldn't find replica %d for partition %d", replica_id, par_id);
  verify(proxy_pair->second);
  return *proxy_pair;
}

std::vector<int> CommunicatorRule::LeadersForPartition(parid_t par_id) const {
  std::vector<int> leaders;
  auto config = Config::GetConfig();
  switch (config->replica_proto_) {
    case MODE_FPGA_RAFT:
      leaders.push_back(0);
      break;
    case MODE_COPILOT:
      leaders.push_back(0);
      leaders.push_back(1);
      break;
    case MODE_MENCIUS:
      for (int replica_id = 0; replica_id < config->GetPartitionSize(par_id); replica_id++)
        leaders.push_back(replica_id);
      break;
    default:
      Log_fatal("Rule mode do not support for this replica protocol now");
      break;
  }
  return leaders;
}

std::vector<SiteProxyPair>
CommunicatorRule::LeaderProxyForPartition(parid_t par_id) const {
  /**
   * ad-hoc. No leader election. fixed leader(id=0) for raft; fixed pilot(id=0) and copilot(id=1) for copilot.
   */
  std::vector<SiteProxyPair> proxy_pairs;
  for (auto leader_id: LeadersForPartition(par_id))
    proxy_pairs.push_back(FindSiteProxyPair(par_id, leader_id));
  return proxy_pairs;  
}

shared_ptr<RuleSpeculativeExecuteQuorumEvent>
CommunicatorRule::BroadcastRuleSpeculativeExecute(shared_ptr<vector<shared_ptr<SimpleCommand>>> vec_piece_data) {
  verify(!vec_piece_data->empty());
  auto par_id = vec_piece_data->at(0)->PartitionId();
  
  shared_ptr<VecPieceData> sp_vpd(new VecPieceData);
  sp_vpd->sp_vec_piece_data_ = vec_piece_data;
  MarshallDeputy md(sp_vpd);

  int n = Config::GetConfig()->GetPartitionSize(par_id);
  int n_leaders = Config::GetConfig()->get_num_leaders(par_id);
  auto e = Reactor::CreateSpEvent<RuleSpeculativeExecuteQuorumEvent>(n, SimpleRWCommand::RuleSuperMajority(n), n_leaders);
  WAN_WAIT;
  for (auto& pair : rpc_par_proxies_[par_id]) {
    rrr::FutureAttr fuattr;
    fuattr.callback =
        [e, this](Future* fu) {
          bool_t accepted;
          value_t result;
          bool_t is_leader;
          fu->get_reply() >> accepted >> result >> is_leader;
          e->FeedResponse(accepted, result, is_leader);
        };
    
    DepId di;
    di.str = "dep";
    di.id = Communicator::global_id++;
    
    auto proxy = pair.second;

#ifdef CURP_TIME_DEBUG
    struct timeval tp;
    gettimeofday(&tp, NULL);
    Log_info("[CURP] [1-] [tx=%d] async_PoorDispatch called by Submit %.3f", tpc_cmd->tx_id_, tp.tv_sec * 1000 + tp.tv_usec / 1000.0);
#endif
    // Record Time
    struct timeval tp;
    gettimeofday(&tp, NULL);
    sp_vpd->time_sent_from_client_ = tp.tv_sec * 1000 + tp.tv_usec / 1000.0;
    
    // Log_info("[CURP] async_CurpPoorDispatch of cmd<%d, %d>", sp_vec_piece->at(0)->client_id_, sp_vec_piece->at(0)->cmd_id_in_client_);
    auto future = proxy->async_RuleSpeculativeExecute(md, fuattr);
    Future::safe_release(future);
  }

  e->Wait();

  return e;
}
    
} // namespace janus
