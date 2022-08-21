// Copyright (C) 2021 Hirain Technologies
// License: Modified BSD Software License Agreement
// Author: Feng DING
// Description: Shared data. What we want is to use a single structure to
//              catch and manage the data transform through out the data pipeline.

#pragma once

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "common/common.h"
#include "framework/event_manager.h"

namespace crdc {
namespace airi {

DECLARE_int32(shared_data_stale_time);

/**
 * @brief This structure is used to store the status of the shared data
 *        It contains three counters.
 */
struct SharedDataStatus {
  std::string to_string() const {
    std::ostringstream oss;
    oss << "counter_add:" << counter_add
        << " counter_remove:" << counter_remove
        << " counter_get:" << counter_get;
    return oss.str();
  }

  uint64_t counter_add = 0;
  uint64_t counter_remove = 0;
  uint64_t counter_get = 0;
};

class SharedData {
 public:
  SharedData() : key_("SharedData") {
    event_manager_ = crdc::airi::common::Singleton<EventManager>::get();
  }
  virtual ~SharedData() = default;
  virtual bool init() = 0;
  /**
   * @brief: this api should clear all the memory used,
   *         and would be called by SharedDataManager when reset DAGStreaming.
   */
  virtual void reset() { CHECK(false) << "reset() not implemented."; }

  /**
   * @brief: remove the stale data if needed.
   */
  virtual void remove_stale_data() { CHECK(false) << "remove_stale_data() not implemented."; }
  virtual std::string name() const = 0;
  virtual size_t size() const = 0;
  const SharedDataStatus stat() const { return stat_; }

  virtual void set_key(const std::string& key) {
    key_ = key;
  }
  virtual std::string key() const { return key_; }

 protected:
  std::string key_;
  mutable SharedDataStatus stat_;
  EventManager* event_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SharedData);
};

REGISTER_COMPONENT(SharedData);
#define REGISTER_SHARED_DATA(name) REGISTER_CLASS(SharedData, name)
}  // namespace airi
}  // namespace crdc
