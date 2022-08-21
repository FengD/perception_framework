// Copyright (C) 2021 Hirain Technologies
// License: Modified BSD Software License Agreement
// Author: Feng DING
// Description: Shared data manager. Like the name this class is used to manage the shared
//              data. The event is the relation ship of the data. And the real data is stored
//              in the cache data herite from shared data.

#include "framework/shared_data_manager.h"

namespace crdc {
namespace airi {

std::ostream& operator<<(std::ostream& os, const SharedDataManager& mgr) {
  os << "All shared_data:" << std::endl;
  for (auto& p : mgr.key_type_map_) {
    os << p.first << " -> " << p.second << std::endl;
  }
  return os;
}

bool SharedDataManager::register_cached_data(const std::string& name, const std::string& type) {
  if (shared_data_map_.find(name) != shared_data_map_.end()) {
    LOG(ERROR) << "Cached Data:" << name << " already registered!";
    return false;
  }
  std::shared_ptr<SharedData> shared_data = SharedDataFactory::get(type);
  if (!shared_data) {
    LOG(ERROR) << "Cached Data:[" << name << "] get error";
    return false;
  }
  shared_data->set_key(name);
  shared_data_map_[name] = shared_data;
  key_type_map_[name] = type;
  return true;
}

bool SharedDataManager::register_frame_cached_data(const std::string& name, size_t freq) {
  if (shared_data_map_.find(name) != shared_data_map_.end()) {
    LOG(ERROR) << "Cached Data:" << name << " already registered!";
    return false;
  }
  std::shared_ptr<SharedData> shared_data(new FrameCachedData(freq));
  if (!shared_data) {
    LOG(ERROR) << "Cached Data:[" << name << "] get error";
    return false;
  }
  shared_data->set_key(name);
  shared_data_map_[name] = shared_data;
  key_type_map_[name] = "FrameCachedData";
  return true;
}

bool SharedDataManager::register_data_event(const std::string& name, const std::string& event) {
  if (shared_data_map_.find(name) == shared_data_map_.end()) {
    LOG(ERROR) << "Cached Data: <" << name << "> not registered!";
    return false;
  }
  event_data_map_[event] = shared_data_map_[name];
  return true;
}

SharedData* SharedDataManager::get_shared_data(const std::string& name) const {
  auto citer = shared_data_map_.find(name);
  if (citer == shared_data_map_.end()) {
    LOG(ERROR) << "SharedData: <" << name << "> NOT REGISTERED!";
    return NULL;
  }
  return citer->second.get();
}

SharedData* SharedDataManager::get_event_data(const std::string& name) const {
  auto citer = event_data_map_.find(name);
  if (citer == event_data_map_.end()) {
    LOG(ERROR) << "SharedData: <" << name << "> NOT REGISTERED!";
    return NULL;
  }
  return citer->second.get();
}

void SharedDataManager::reset() {
  for (auto& shared_data : shared_data_map_) {
    shared_data.second->reset();
  }
  LOG(INFO) << "reset all SharedData. nums: " << shared_data_map_.size();
}

void SharedDataManager::remove_stale_data() {
  for (auto& shared_data : shared_data_map_) {
    shared_data.second->remove_stale_data();
  }
  LOG(INFO) << "remove stale SharedData. nums: " << shared_data_map_.size();
}

}  // namespace airi
}  // namespace crdc

