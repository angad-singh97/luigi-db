#include "service.h"

namespace janus {

CopilotPlusServiceImpl::CopilotPlusServiceImpl(TxLogServer *svr): svr_((CopilotPlusServer*) svr) {
  
}

void CopilotPlusServiceImpl::Submit(const MarshallDeputy& cmd,
              slotid_t* i,
              slotid_t* j,
              ballot_t* ballot,
              rrr::DeferredReply* defer) {
  verify(svr_);
  svr_->OnSubmit(cmd,
                  i,
                  j,
                  ballot,
                  bind(&rrr::DeferredReply::reply, defer));
}

void CopilotPlusServiceImpl::FrontRecover(const MarshallDeputy& cmd,
                                          const slotid_t& i,
                                          const slotid_t& j,
                                          const ballot_t& ballot,
                                          bool_t* accept_recover,
                                          rrr::DeferredReply* defer) {
  verify(svr_);
  svr_->OnFrontRecover(cmd,
                        i,
                        j,
                        ballot,
                        accept_recover,
                        bind(&rrr::DeferredReply::reply, defer));
}

void CopilotPlusServiceImpl::FrontCommit(const MarshallDeputy& cmd,
                                          const slotid_t& i,
                                          const slotid_t& j,
                                          const ballot_t& ballot,
                                          rrr::DeferredReply* defer) {
  verify(svr_);
  svr_->OnFrontCommit(cmd,
                      i,
                      j,
                      ballot,
                      bind(&rrr::DeferredReply::reply, defer));
}

}