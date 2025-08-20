#pragma once

#include "__dep__.h"
#include "constants.h"
#include <vector>
#include <sstream>

namespace janus {

class View {
 public:
  int n_; // number of replicas in this view
  std::vector<int> leaders_; // leader ids (size = 1 for Raft since raft only have 1 leader)
  epoch_t view_id_; // corresponding term in Raft
  
  View() : n_(0), view_id_(0) {}
  
  View(int n, const std::vector<int>& leaders, epoch_t view_id) 
      : n_(n), leaders_(leaders), view_id_(view_id) {}
  
  View(int n, int leader, epoch_t view_id) 
      : n_(n), leaders_{leader}, view_id_(view_id) {}
  
  bool operator==(const View& other) const {
    return n_ == other.n_ && leaders_ == other.leaders_ && view_id_ == other.view_id_;
  }
  
  bool operator!=(const View& other) const {
    return !(*this == other);
  }
  
  bool IsEmpty() const {
    return n_ == 0 && leaders_.empty();
  }
  
  int GetLeader() const {
    return leaders_.empty() ? -1 : leaders_[0];
  }
  
  std::string ToString() const {
    std::stringstream ss;
    ss << "View{n=" << n_ << ", view_id=" << view_id_ << ", leaders=[";
    for (size_t i = 0; i < leaders_.size(); i++) {
      if (i > 0) ss << ",";
      ss << leaders_[i];
    }
    ss << "]}";
    return ss.str();
  }
};

} // namespace janus