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
CommunicatorRule::BroadcastRuleSpeculativeExecute(shared_ptr<vector<shared_ptr<SimpleCommand>>> vec_piece_data,
                            Coordinator *coo) {
  verify(!vec_piece_data->empty());
  auto par_id = vec_piece_data->at(0)->PartitionId();
  
  shared_ptr<VecPieceData> sp_vpd(new VecPieceData);
  sp_vpd->sp_vec_piece_data_ = vec_piece_data;
  MarshallDeputy md(sp_vpd);

  int n_leaders = Config::GetConfig()->get_num_leaders(par_id);
  auto e = Reactor::CreateSpEvent<RuleSpeculativeExecuteQuorumEvent>(n_leaders, n_leaders);
  WAN_WAIT;
  for (auto& pair : rpc_par_proxies_[par_id]) {
    rrr::FutureAttr fuattr;
    fuattr.callback =
        [e, this](Future* fu) {
          bool_t accepted;
          value_t result;
          fu->get_reply() >> accepted >> result;
          e->FeedResponse(accepted, result);
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

void CommunicatorRule::BroadcastDispatch(shared_ptr<vector<shared_ptr<SimpleCommand>>> sp_vec_piece,
                                                Coordinator *coo,
                                                const std::function<void(int res, TxnOutput &)> &callback) {
  cmdid_t cmd_id = sp_vec_piece->at(0)->root_id_;
  verify(!sp_vec_piece->empty());
  auto par_id = sp_vec_piece->at(0)->PartitionId();

  auto pair_proxies = LeaderProxyForPartition(par_id);
  Log_debug("send dispatch to site %d, %d", pair_proxies[0].first,
            pair_proxies[1].first);
  shared_ptr<VecPieceData> sp_vpd(new VecPieceData);
  sp_vpd->sp_vec_piece_data_ = sp_vec_piece;
  MarshallDeputy md(sp_vpd);

  struct DepId di;
  di.id = cmd_id;
  di.str = __func__;

  dispatch_quota.WaitUntilGreaterOrEqualThan(0);

  bool send = false;

  for (auto leader_id: LeadersForPartition(par_id)) {
    rrr::FutureAttr fuattr;
    fuattr.callback = [coo, this, callback, leader_id](Future *fu) {
      int32_t ret;
      TxnOutput outputs;
      fu->get_reply() >> ret >> outputs;
      n_pending_rpc_[leader_id]--;
      verify(n_pending_rpc_[leader_id] >= 0);
      dispatch_quota.Set(dispatch_quota.value_ + 1);
      callback(ret, outputs);
    };

    if (n_pending_rpc_[leader_id] < max_pending_rpc_) {
      auto future =
          pair_proxies[leader_id].second->async_Dispatch(cmd_id, di, md, fuattr);
      Future::safe_release(future);
      n_pending_rpc_[leader_id]++;
      dispatch_quota.Set(dispatch_quota.value_ - 1);
      send = true;
    }
  }

  verify(send);
}
    
} // namespace janus
