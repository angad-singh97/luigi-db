#include "commo.h"

namespace janus {

MongodbCommo::MongodbCommo(PollMgr* poll) : Communicator(poll) {
//  verify(poll != nullptr);
}

}