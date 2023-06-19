
#include "service.h"
#include "server.h"

namespace janus {

MultiPaxosPlusServiceImpl::MultiPaxosPlusServiceImpl(TxLogServer *sched)
    : sched_((PaxosPlusServer*)sched) {

}


void MultiPaxosPlusServiceImpl::Forward(const MarshallDeputy& pos,
                                        const MarshallDeputy& cmd,
                                        const bool_t& accepted,
                                        rrr::DeferredReply* defer) {
  verify(sched_ != nullptr);
  sched_->OnForward(dynamic_pointer_cast<Position>(const_cast<MarshallDeputy&>(pos).sp_data_),
                    const_cast<MarshallDeputy&>(cmd).sp_data_,
                    accepted);
  defer->reply();
}

void MultiPaxosPlusServiceImpl::CoordinatorAccept(const MarshallDeputy& pos,
                                                  const MarshallDeputy& cmd,
                                                  bool_t* accepted,
                                                  rrr::DeferredReply* defer) {
  verify(sched_ != nullptr);
  sched_->OnCoordinatorAccept(dynamic_pointer_cast<Position>(const_cast<MarshallDeputy&>(pos).sp_data_),
                              const_cast<MarshallDeputy&>(cmd).sp_data_,
                              accepted,
                              bind(&rrr::DeferredReply::reply, defer));
}

void MultiPaxosPlusServiceImpl::Prepare(const MarshallDeputy& pos,
            const ballot_t& ballot,
            bool_t* accepted,
            ballot_t* seen_ballot,
            rrr::i32* last_accepted_status,
            MarshallDeputy* last_accepted_cmd,
            ballot_t* last_accepted_ballot,
            rrr::DeferredReply* defer) {
  verify(sched_ != nullptr);
  // TODO: correct for last_accepted_cmd?
  sched_->OnPrepare(dynamic_pointer_cast<Position>(const_cast<MarshallDeputy&>(pos).sp_data_),
                    ballot,
                    accepted,
                    seen_ballot,
                    last_accepted_status,
                    const_cast<MarshallDeputy&>(*last_accepted_cmd).sp_data_,
                    last_accepted_ballot,
                    bind(&rrr::DeferredReply::reply, defer));
}

void MultiPaxosPlusServiceImpl::Accept(const MarshallDeputy& pos,
            const MarshallDeputy& cmd,
            const ballot_t& ballot,
            bool_t* accepted,
            ballot_t* seen_ballot,
            rrr::DeferredReply* defer) {
  verify(sched_ != nullptr);
  sched_->OnAccept(dynamic_pointer_cast<Position>(const_cast<MarshallDeputy&>(pos).sp_data_),
                    const_cast<MarshallDeputy&>(cmd).sp_data_,
                    ballot,
                    accepted,
                    seen_ballot,
                    bind(&rrr::DeferredReply::reply, defer));
}

void MultiPaxosPlusServiceImpl::Commit(const MarshallDeputy& pos,
            const MarshallDeputy& cmd,
            rrr::DeferredReply* defer) {
  verify(sched_ != nullptr);
  sched_->OnCommit(dynamic_pointer_cast<Position>(const_cast<MarshallDeputy&>(pos).sp_data_),
                    const_cast<MarshallDeputy&>(cmd).sp_data_);
  defer->reply();
}


} // namespace janus;
