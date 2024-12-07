#pragma once

#include "deptran/communicator.h"

namespace janus
{

class CommunicatorRule : public Communicator {
public:
    unordered_map<int, uint32_t> n_pending_rpc_;
    const uint32_t max_pending_rpc_ = 200;
    SharedIntEvent dispatch_quota{};

    CommunicatorRule(PollMgr* poll_mgr = nullptr)
     :Communicator(poll_mgr) {
        dispatch_quota.value_ = 3 * max_pending_rpc_;
    }

    std::vector<int> LeadersForPartition(parid_t par_id) const;

    SiteProxyPair FindSiteProxyPair(parid_t par_id, int replica_id) const;

    std::vector<SiteProxyPair>
    LeaderProxyForPartition(parid_t par_id) const;

    shared_ptr<RuleSpeculativeExecuteQuorumEvent>
    BroadcastRuleSpeculativeExecute(shared_ptr<vector<shared_ptr<SimpleCommand>>> vec_piece_data);

    void BroadcastDispatch(shared_ptr<vector<shared_ptr<SimpleCommand>>> vec_piece_data,
                           Coordinator *coo,
                           const std::function<void(int res, TxnOutput &)> &) override;

};
    
} // namespace janus
