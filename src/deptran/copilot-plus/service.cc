#include "service.h"

namespace janus {

CopilotPlusServiceImpl::CopilotPlusServiceImpl(TxLogServer *svr): svr_((CopilotPlusServer*) svr) {
  
}

void CopilotPlusServiceImpl::Submit(const slotid_t& slot_id,
                                    const MarshallDeputy& cmd,
                                    bool_t* accepted,
                                    slotid_t* i,
                                    slotid_t* j,
                                    ballot_t* ballot,
                                    rrr::DeferredReply* defer) {
  Log_info("[copilot+] enter Submit");
  verify(svr_);
  svr_->OnSubmit(slot_id,
                  const_cast<MarshallDeputy&>(cmd).sp_data_,
                  accepted,
                  i,
                  j,
                  ballot,
                  bind(&rrr::DeferredReply::reply, defer));
  Log_info("[copilot+] exit Submit");
}

void CopilotPlusServiceImpl::FrontRecover(const slotid_t& slot_id,
                                          const MarshallDeputy& cmd,
                                          const bool_t& commit_no_op_,
                                          const slotid_t& i,
                                          const slotid_t& j,
                                          const ballot_t& ballot,
                                          bool_t* accept_recover,
                                          rrr::DeferredReply* defer) {
  verify(svr_);
  svr_->OnFrontRecover(slot_id,
                        const_cast<MarshallDeputy&>(cmd).sp_data_,
                        commit_no_op_,
                        i,
                        j,
                        ballot,
                        accept_recover,
                        bind(&rrr::DeferredReply::reply, defer));
}

void CopilotPlusServiceImpl::FrontCommit(const slotid_t& slot_id,
                                          const MarshallDeputy& cmd,
                                          const bool_t& commit_no_op_,
                                          const slotid_t& i,
                                          const slotid_t& j,
                                          const ballot_t& ballot,
                                          rrr::DeferredReply* defer) {
  verify(svr_);
  svr_->OnFrontCommit(slot_id,
                      const_cast<MarshallDeputy&>(cmd).sp_data_,
                      commit_no_op_, i,
                      j,
                      ballot,
                      bind(&rrr::DeferredReply::reply, defer));
}

}