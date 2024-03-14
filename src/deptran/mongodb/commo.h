#pragma once

#include "../communicator.h"

namespace janus {

class MongodbCommo : public Communicator {
 public:
  MongodbCommo() = delete;
  MongodbCommo(PollMgr*);
};

} // namespace janus
