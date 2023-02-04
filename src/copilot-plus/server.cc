#include "../__dep__.h"
#include "server.h"
#include "frame.h"

namespace janus {


CopilotPlusServer::CopilotPlusServer(Frame *frame) {
  frame_ = frame;
  Setup();
} 

CopilotPlusServer::~CopilotPlusServer() {

}

void CopilotPlusServer::OnSubmit(const MarshallDeputy& cmd,
                                  slotid_t* i,
                                  slotid_t* j,
                                  ballot_t* ballot,
                                  const function<void()> &cb) {
  //TODO
}

void CopilotPlusServer::OnFrontRecover(const MarshallDeputy& cmd,
                                        const slotid_t& i,
                                        const slotid_t& j,
                                        const ballot_t& ballot,
                                        bool_t* accept_recover,
                                        const function<void()> &cb) {
  //TODO
}

void CopilotPlusServer::OnFrontCommit(const MarshallDeputy& cmd,
                    const slotid_t& i,
                    const slotid_t& j,
                    const ballot_t& ballot,
                    const function<void()> &cb) {
  //TODO
}

void CopilotPlusServer::Setup() {
  //TODO
}


};