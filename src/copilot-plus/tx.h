#pragma once

#include "../__dep__.h"
#include "../marshallable.h"

namespace janus {

class KeyValueCommand: public Marshallable {
 public:
  key_t key_;
  KeyValueCommand(int32_t k): Marshallable(MarshallDeputy::CMD_KV) {};
  Marshal& ToMarshal(Marshal& m) const override;
  Marshal& FromMarshal(Marshal& m) override;
};


}