// Copyright (C) 2021 FengD
// License: Modified BSD Software License Agreement
// Author: Feng DING
// Description: Shared data manager. Like the name this class is used to manage the shared
//              data. The event is the relation ship of the data. And the real data is stored
//              in the cache data herite from shared data.

#pragma once

#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>
#include "framework/cached_data.h"
#include "framework/shared_data.h"

namespace crdc {
namespace airi {

class SharedDataManager {
 public:
  SharedDataManager() = default;
  ~SharedDataManager() = default;

  bool register_cached_data(const std::string& name, const std::string& type);
  template <typename T>
  bool register_cached_data(const std::string& name, int freq) {
    if (shared_data_map_.find(name) != shared_data_map_.end()) {
      LOG(ERROR) << "CachedData:" << name << " already registered!";
      return false;
    }
    std::shared_ptr<SharedData> shared_data(new CachedData<T>(freq));
    if (!shared_data) {
      LOG(ERROR) << "CachedData:[" << name << "] get error";
      return false;
    }
    shared_data->set_key(name);
    shared_data_map_[name] = shared_data;
    key_type_map_[name] = "CachedData";
    return true;
  }
  bool register_frame_cached_data(const std::string& name, size_t freq);
  bool register_data_event(const std::string& name, const std::string& event);
  SharedData* get_shared_data(const std::string& name) const;
  SharedData* get_event_data(const std::string& name) const;
  void reset();
  void remove_stale_data();

  friend std::ostream& operator<<(std::ostream& os, const SharedDataManager& mgr);

 private:
  friend class crdc::airi::common::Singleton<SharedDataManager>;

  using SharedDataMap = std::unordered_map<std::string, std::shared_ptr<SharedData>>;
  SharedDataMap event_data_map_;
  SharedDataMap shared_data_map_;
  std::unordered_map<std::string, std::string> key_type_map_;
  bool inited_ = false;
  DISALLOW_COPY_AND_ASSIGN(SharedDataManager);
};

std::ostream& operator<<(std::ostream& os, const SharedDataManager& mgr);

}  // namespace airi
}  // namespace crdc
