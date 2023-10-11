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
class CurpServiceImpl : virtual public CurpService{
 public:
  TxLogServer* sched_;
  CurpServiceImpl() {};
  // CurpServiceImpl(TxLogServer* sched);

  void CurpDispatch(const int32_t& client_id,
                    const int32_t& cmd_id_in_client,
                    const MarshallDeputy& cmd,
                    bool_t* accepted,
                    ver_t* ver,
                    int32_t* result,
                    int32_t* finish_countdown,
                    int32_t* key_hotness,
                    siteid_t* coo_id,
                    rrr::DeferredReply* defer) override;

  void CurpWaitCommit(const int32_t& client_id,
                  const int32_t& cmd_id_in_client,
                  bool_t* committed,
                  value_t* commit_result,
                  rrr::DeferredReply* defer) override;

  void CurpForward(const bool_t& accepted,
                    const ver_t& ver,
                    const MarshallDeputy& cmd,
                    rrr::DeferredReply* defer) override;

  void CurpPrepare(const key_t& k,
                    const ver_t& ver,
                    const ballot_t& ballot,
                    bool_t* accepted,
                    rrr::i32* status,
                    ballot_t* last_accepted_ballot,
                    MarshallDeputy* cmd,
                    rrr::DeferredReply* defer);

  void CurpAccept(const ver_t& ver,
                  const ballot_t& ballot,
                  const MarshallDeputy& md_cmd,
                  bool_t* accepted,
                  ballot_t* seen_ballot,
                  rrr::DeferredReply* defer);
  
  void CurpCommit(const ver_t& ver,
                  const MarshallDeputy& md_cmd,
                  rrr::DeferredReply* defer) override;

//   void OriginalSubmit(const MarshallDeputy& cmd,
//                     const rrr::i64& dep_id,
//                     bool_t* slow,
//                     rrr::DeferredReply* defer) override;

  void CurpTest(const int32_t& a,
                int32_t* b,
                rrr::DeferredReply* defer) override;

};

} // namespace janus
