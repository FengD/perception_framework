// Copyright (C) 2021 FengD
// License: Modified BSD Software License Agreement
// Author: Feng DING
// Description: Event is the minimium unit of description task
//              Event is used to discribre the info and EventMeta contains the reletions
//              between each node. Event is the relationship.

#pragma once

#include <sstream>
#include <string>
#include <iomanip>

#define GLOG_TIMESTAMP(timestamp) std::fixed << std::setprecision(9) << timestamp

namespace crdc {
namespace airi {

using EventID = int;
using OperatorID = int;
using WorkerID = int;

/**
 * @brief 
 * Event is used to note the event info
 */ 
struct Event {
  EventID event_id = 0;
  uint64_t timestamp = 0LL;
  // local timestamp to compute process delay.
  uint64_t local_timestamp = 0LL;
  // this is a reserved variable which could be used to store other info
  std::string reserve;
  Event(): event_id(0), timestamp(0LL), local_timestamp(0LL) {}
  std::string to_string() const {
    std::ostringstream oss;
    oss << "event_id: " << event_id
        << " timestamp: " << GLOG_TIMESTAMP(timestamp)
        << " reserve: " << reserve;
    return oss.str();
  }
};

/**
 * @brief 
 * EventMeta is used to note the event info and the work which used the event
 */ 
struct EventMeta {
  EventID event_id = 0;
  WorkerID to_node = 0;
  WorkerID from_node = 0;
  std::string name;
  EventMeta(): event_id(0), to_node(0), from_node(0) {}
  std::string to_string() const {
    std::ostringstream oss;
    oss << "event_id: " << event_id << " name: '" << name
      << "' from_node: " << from_node << " to_node: " << to_node;
    return oss.str();
  }
};

}  // namespace airi
}  // namespace crdc
