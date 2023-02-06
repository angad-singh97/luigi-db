#include "../__dep__.h"
#include "tx.h"

namespace janus {

Marshal& KeyValueCommand::ToMarshal(Marshal& m) const {
  m << key_;
  return m;
}

Marshal& KeyValueCommand::FromMarshal(Marshal& m) {
  m >> key_;
  return m;
}

Marshal& NoOpCommand::ToMarshal(Marshal& m) const {
  return m;
}

Marshal& NoOpCommand::FromMarshal(Marshal& m) {
  return m;
}

}