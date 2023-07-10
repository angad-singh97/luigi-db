#include <memory>

#pragma once

#include "__dep__.h"
#include "marshallable.h"

namespace janus {

class Position : public Marshallable {
 public:
  int32_t len_{0};
  vector<pos_t> pos_;

  Position(MarshallDeputy::Kind kind, int32_t len) : Marshallable(kind), len_(len) {
    for (int i = 0; i < len_; i++) {
      pos_.push_back(-1);
    }
  }

  Position(const Position& other) : Marshallable(other.kind_), len_(other.len_) {
    for (int i = 0; i < other.len_; i++) {
      pos_.push_back(other.pos_[i]);
    }
  }

  Marshal& ToMarshal(Marshal& m) const override {
    m << len_;
    for (int i = 0; i < len_; i++) {
      m << pos_[i];
    }
    return m;
  }

  Marshal& FromMarshal(Marshal& m) override {
    m >> len_;
    for (int i = 0; i < len_; i++) {
      m >> pos_[i];
    }
    return m;
  }

  bool operator < (const Position &other) const {
    verify(len_ == other.len_);
    for (int i = 0; i < len_; i++) {
      if (pos_[i] != other.pos_[i]) {
        return pos_[i] < other.pos_[i];
      }
    }
    return false;
  }

  bool operator == (const Position &other) const {
    verify(len_ == other.len_);
    for (int i = 0; i < len_; i++) {
      if (pos_[i] != other.pos_[i]) {
        return false;
      }
    }
    return true;
  }

  void set(int pos, pos_t value) {
    verify(pos <= pos_.size());
    pos_[pos] = value;
  }

  pos_t get(int pos) const {
    verify(pos <= pos_.size());
    return pos_[pos];
  }
};

class ClassicPosition: public Position {
 public:
  slotid_t i, j;

  ClassicPosition() : Position(MarshallDeputy::POSITION_CLASSIC, 2) {
  }

};

class CopilotPosition: public Position {
 public:

	// y -> position on pilot log, n -> position on copilot log
	slotid_t y_i, y_j, n_i, n_j;

  CopilotPosition() : Position(MarshallDeputy::POSITION_COPILOT, 4) {
  }

};

}

