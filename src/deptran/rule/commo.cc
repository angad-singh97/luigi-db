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
