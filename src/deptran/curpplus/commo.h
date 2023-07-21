#pragma once

#include "../__dep__.h"
#include "../constants.h"
#include "../communicator.h"
#include "../position.h"
#include "server.h"
#include <chrono>
#include <ctime>

namespace janus {



class CurpPlusCommo : public Communicator {
  public:
  CurpPlusCommo() = delete;
  CurpPlusCommo(PollMgr*);

  // shared_ptr<CurpDispatchQuorumEvent>
  // CurpBroadcastDispatch(shared_ptr<vector<shared_ptr<SimpleCommand>>> vec_piece_data);

  // shared_ptr<CurpDispatchQuorumEvent>
  // CurpBroadcastDispatch(shared_ptr<Marshallable> cmd);

  // shared_ptr<QuorumEvent>
  // DirectCurpBroadcastWaitCommit(shared_ptr<vector<shared_ptr<SimpleCommand>>> vec_piece_data,
  //                             siteid_t coo_id);

  // shared_ptr<QuorumEvent>
  // DirectCurpBroadcastWaitCommit(shared_ptr<Marshallable> cmd,
  //                             siteid_t coo_id);

  // shared_ptr<IntEvent>
  // ForwardResultToCoordinator(parid_t par_id,
  //                           const shared_ptr<Marshallable>& cmd,
  //                           Position pos,
  //                           bool_t accepted);
  
  // shared_ptr<CurpPlusCoordinatorAcceptQuorumEvent>
  // BroadcastCoordinatorAccept(parid_t par_id,
  //                           shared_ptr<Position> pos,
  //                           shared_ptr<Marshallable> cmd);

  // shared_ptr<CurpPlusPrepareQuorumEvent>
  // BroadcastPrepare(parid_t par_id,
  //                   shared_ptr<Position> pos,
  //                   ballot_t ballot);

  // shared_ptr<CurpPlusAcceptQuorumEvent>
  // BroadcastAccept(parid_t par_id,
  //                 shared_ptr<Position> pos,
  //                 shared_ptr<Marshallable> cmd,
  //                 ballot_t ballot);

  // shared_ptr<IntEvent>
  // BroadcastCommit(parid_t par_id,
  //                 shared_ptr<Position> pos,
  //                 shared_ptr<Marshallable> md_cmd,
  //                 uint16_t ban_site);
  
};

} // namespace janus
