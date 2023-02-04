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

void CopilotPlusServer::Setup() {

}

};