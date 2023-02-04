#include "../__dep__.h"
#include "coordinator.h"

namespace janus {

CopilotPlusCoordinator::CopilotPlusCoordinator(uint32_t coo_id,
              int benchmark,
              ClientControlServiceImpl *ccsi = NULL,
              uint32_t thread_id = 0) 
  :Coordinator(coo_id, benchmark, ccsi, thread_id){

}

CopilotPlusCoordinator::~CopilotPlusCoordinator() {
}

void CopilotPlusCoordinator::DoTxAsync(TxRequest &) {

}

void CopilotPlusCoordinator::Restart() {

}

}