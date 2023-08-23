#pragma once

#include "__dep__.h"
#include "constants.h"
#include "rcc/graph.h"
#include "rcc/graph_marshaler.h"
#include "command.h"
#include "procedure.h"
#include "command_marshaler.h"
#include "rcc_rpc.h"
#include <chrono>

class SimpleCommand;
namespace janus {

class TxLogServer;
class MenciusPlusServer;
class CurpServiceImpl : virtual public CurpService{
 public:
  TxLogServer* sched_;
  CurpServiceImpl() {};
  // CurpServiceImpl(TxLogServer* sched);

  void CurpPoorDispatch(const int32_t& client_id,
                    const int32_t& cmd_id_in_client,
                    const MarshallDeputy& cmd,
                    bool_t* accepted,
                    pos_t* pos0,
                    pos_t* pos1,
                    int32_t* result,
                    siteid_t* coo_id,
                    rrr::DeferredReply* defer) override;

  void CurpWaitCommit(const int32_t& client_id,
                  const int32_t& cmd_id_in_client,
                  bool_t* committed,
                  rrr::DeferredReply* defer) override;

  void CurpForward(const MarshallDeputy& pos,
                const MarshallDeputy& cmd,
                const bool_t& accepted,
                rrr::DeferredReply* defer) override;

  void CurpCoordinatorAccept(const MarshallDeputy& pos,
                          const MarshallDeputy& cmd,
                          bool_t* accepted, rrr::DeferredReply* defer) override;

  void CurpPrepare(const MarshallDeputy& pos,
              const ballot_t& ballot,
              bool_t* accepted,
              ballot_t* seen_ballot,
              rrr::i32* last_accepted_status,
              MarshallDeputy* last_accepted_cmd,
              ballot_t* last_accepted_ballot,
              rrr::DeferredReply* defer) override;

  void CurpAccept(const MarshallDeputy& pos,
              const MarshallDeputy& md_cmd,
              const ballot_t& ballot,
              bool_t* accepted,
              ballot_t* seen_ballot,
              rrr::DeferredReply* defer) override;
  
  void CurpCommit(const MarshallDeputy& pos,
              const MarshallDeputy& md_cmd,
              rrr::DeferredReply* defer) override;

//   void OriginalSubmit(const MarshallDeputy& cmd,
//                     const rrr::i64& dep_id,
//                     bool_t* slow,
//                     rrr::DeferredReply* defer) override;

  void CurpTest(const int32_t& a,
                int32_t* b,
                rrr::DeferredReply* defer) override;
  
  void CurpProposeFinish(const int32_t& key,
                          uint64_t* pos,
                          rrr::DeferredReply* defer) override;
  
  void CurpCommitFinish(const MarshallDeputy& pos,
                        rrr::DeferredReply* defer) override;
};

} // namespace janus
