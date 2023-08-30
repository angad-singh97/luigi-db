#include "exec.h"

namespace janus {


ballot_t FpgaRaftPlusExecutor::Prepare(const ballot_t ballot) {
  verify(0);
  return 0;
}

ballot_t FpgaRaftPlusExecutor::Accept(const ballot_t ballot,
                                    shared_ptr<Marshallable> cmd) {
  verify(0);
  return 0;
}

ballot_t FpgaRaftPlusExecutor::AppendEntries(const ballot_t ballot,
                                         shared_ptr<Marshallable> cmd) {
  verify(0);
  return 0;
}

ballot_t FpgaRaftPlusExecutor::Decide(ballot_t ballot, CmdData& cmd) {
  verify(0);
  return 0;
}

} // namespace janus
