#pragma once

#include "../communicator.h"

namespace janus {

class MongodbCommo : public Communicator {
 public:
  MongodbCommo() = delete;
  MongodbCommo(rusty::Arc<rrr::PollThreadWorker>);

  void BroadcastCommit(const parid_t par_id,
                        const shared_ptr<Marshallable> cmd);
};

} // namespace janus
