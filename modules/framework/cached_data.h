// Copyright (C) 2021 Hirain Technologies
// License: Modified BSD Software License Agreement
// Author: Feng DING
// Description: cached data

#pragma once

#include <algorithm>
#include <deque>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "framework/shared_data.h"
#include "framework/frame.h"

namespace crdc {
namespace airi {

DECLARE_int32(cached_data_stale_time);
DECLARE_int32(cached_data_tolerate_offset);

template <class T>
class StaticCachedData;
template <class T>
class DynamicCachedData;
template <class T>
using SharedPtr = std::shared_ptr<T>;

template <class T>
class CachedDataBase : public SharedData {
 public:
  CachedDataBase() : SharedData() { stale_time_ = FLAGS_cached_data_stale_time * 1e6; }
  virtual ~CachedDataBase() = default;

  bool init() override { return true; }

  /**
   * @brief get the frequency
   */
  virtual size_t hz() const = 0;

  /**
   * @brief get the period in usec
   */
  virtual size_t uperiod() const = 0;

  /**
   * @brief remove the stale data
   */
  void remove_stale_data() override {}

  /**
   * @brief remove the stale data out of stale_time
   * @param [in] stale time
   */
  virtual void remove_stale_data(const uint64_t& stale_time) {}

  /**
   * @brief get the data in the cache by key in tolerate
   * @param [in]key word
   * @param [out] the data
   * @param [in] tolerete time
   * @return is the action success[bool]
   */
  virtual bool get(uint64_t key, SharedPtr<T>* data,
                   int tolerate = FLAGS_cached_data_tolerate_offset) const = 0;

  /**
   * @brief get the newest data in the cache
   * @param [out] the data
   * @return is the action success[bool]
   */
  virtual bool get_newest(SharedPtr<const T>* data) const = 0;

  /**
   * @brief get the data in the cache byfrom and to
   * @param [in]from key
   * @param [in]to key
   * @param [out] the data
   * @return is the action success[bool]
   */
  virtual bool get(uint64_t from, uint64_t to, std::vector<SharedPtr<const T>>* data) const = 0;

  /**
   * @brief put the data in the cache
   */
  virtual bool put(uint64_t key, const std::shared_ptr<T>& data) = 0;
  virtual bool put(uint64_t key, const T& data) = 0;

 protected:
  uint64_t stale_time_;
};

template <class T>
class CachedData : public CachedDataBase<T> {
 public:
  CachedData() : CachedDataBase<T>() {}
  explicit CachedData(int hz) : CachedData() {
    if (hz > 0) {
      impl_.reset(new StaticCachedData<T>(hz));
    } else {
      impl_.reset(new DynamicCachedData<T>());
    }
  }
  virtual ~CachedData() = default;

  size_t hz() const override { return impl_->hz(); }
  size_t uperiod() const override { return impl_->uperiod(); }
  size_t size() const override { return impl_->size(); }

  void set_key(const std::string& key) override {
    SharedData::set_key(key);
    impl_->set_key(key);
  }
  std::string key() const override { return impl_->key(); }

  std::string name() const override { return impl_->name(); }

  bool init() override { return impl_->init(); }

  void reset() override { impl_->reset(); }

  bool get(uint64_t key, SharedPtr<T>* data,
           int tolerate = FLAGS_cached_data_tolerate_offset) const override {
    if (!impl_->get(key, data, tolerate)) {
      LOG(ERROR) << "Failed to get [" << this->key() << "]";
      return false;
    }
    return true;
  }

  bool get_newest(SharedPtr<const T>* data) const override {
    if (!impl_->get_newest(data)) {
      // LOG_EVERY_N(ERROR, 100) << "Failed to get_newest [" << this->key() << "]";
      return false;
    }
    return true;
  }

  bool get(uint64_t from, uint64_t to, std::vector<SharedPtr<const T>>* data) const override {
    if (to <= from) {
      LOG(ERROR) << "Failed to get [" << this->key() << "]: from >= to ("
                 << from << " >= " << to << ")";
      return false;
    }
    data->clear();
    if (!impl_->get(from, to, data)) {
      LOG(ERROR) << "Failed to get [" << this->key() << "]";
      return false;
    }
    this->stat_.counter_get += data->size();
    return true;
  }

  bool put(uint64_t key, const std::shared_ptr<T>& data) override {
    if (!impl_->put(key, data)) {
      return false;
    }
    return true;
  }
  bool put(uint64_t key, const T& data) override {
    if (!impl_->put(key, data)) {
      return false;
    }
    return true;
  }

  void publish(const uint64_t& timestamp, const Event* sub_event,
               const std::vector<EventMeta>& pub_events,
               const std::vector<int>* processed = nullptr) {
    for (const auto& event_meta : pub_events) {
      Event event;
      event.event_id = event_meta.event_id;
      event.timestamp = timestamp;
      this->event_manager_->publish(event);
    }
  }

  void publish_data_and_event(const uint64_t& timestamp, const SharedPtr<T>& data,
                              const Event* sub_event, const std::vector<EventMeta>& pub_events,
                              const std::vector<int>* processed = nullptr) {
    if (this->put(timestamp, data)) {
      publish(timestamp, sub_event, pub_events, processed);
    }
  }

  void publish_data_and_event(const uint64_t& timestamp, const SharedPtr<T>& data,
                              const Event* sub_event, const EventMeta& pub_event,
                              const std::vector<int>* processed = nullptr) {
    if (data) {
      if (!this->put(timestamp, data)) {
        return;
      }
    }
    Event event;
    event.event_id = pub_event.event_id;
    event.timestamp = timestamp;
    this->event_manager_->publish(event);
  }

  void remove_stale_data() override {
    impl_->remove_stale_data(this->stale_time_);
  }

 private:
  std::unique_ptr<CachedDataBase<T>> impl_;
  DISALLOW_COPY_AND_ASSIGN(CachedData);
};

template <class T>
class DynamicCachedData : public CachedDataBase<T> {
 public:
  DynamicCachedData() : CachedDataBase<T>(), hz_(0), last_(0), latest_(0) {}

  size_t hz() const override {
    std::unique_lock<std::mutex> lock(lock_);
    uint64_t slot = last_ / slot_size_;
    if (data_.find(slot) == data_.end()) {
      return 0;
    }
    return data_.at(slot).size();
  }
  size_t uperiod() const override {
    size_t hz = this->hz();
    if (hz == 0) {
      return 0;
    }
    return slot_size_ / this->hz();
  }

  size_t size() const override {
    int size = 0;
    std::unique_lock<std::mutex> lock(lock_);
    for (auto& p : data_) {
      size += p.second.size();
    }
    return size * sizeof(T);
  }

  void reset() override {
    std::unique_lock<std::mutex> lock(lock_);
    data_.clear();
  }

  std::string name() const override { return "DynamicCachedData"; }

  bool get(uint64_t key, std::shared_ptr<T>* data,
           int tolerate = FLAGS_cached_data_tolerate_offset) const override {
    std::unique_lock<std::mutex> lock(lock_);
    uint64_t slot = key / slot_size_;
    if (data_.find(slot) != data_.end() && data_.at(slot).find(key) != data_.at(slot).end()) {
      *data = data_.at(slot).at(key);
      return true;
    }
    bool found = false;
    if (tolerate > 0) {
      uint64_t tolerate_diff = 1000 * tolerate;
      uint64_t diff = std::numeric_limits<uint64_t>::max();
      for (uint64_t s = slot - 1; s < slot + 2; ++s) {
        if (data_.find(s) == data_.end()) {
          continue;
        }
        for (auto& p : data_.at(s)) {
          uint64_t dt = (key > p.first ? key - p.first : p.first - key);
          if (dt < diff && dt < tolerate_diff) {
            diff = dt;
            found = true;
            *data = data_.at(s).at(p.first);
          }
        }
      }
      this->stat_.counter_get += found;
    }
    return found;
  }

  bool get_newest(SharedPtr<const T>* data) const override {
    std::unique_lock<std::mutex> lock(lock_);
    uint64_t slot = latest_ / slot_size_;
    if (data_.find(slot) != data_.end() && data_.at(slot).find(latest_) != data_.at(slot).end()) {
      *data = data_.at(slot).at(latest_);
      ++this->stat_.counter_get;
      return true;
    }
    return false;
  }

  bool get(uint64_t from, uint64_t to, std::vector<SharedPtr<const T>>* data) const override {
    std::unique_lock<std::mutex> lock(lock_);
    if (data_.empty()) {
      LOG(WARNING) << this->key() << "CachedData: empty";
      return false;
    }

    for (auto it = data_.begin(); it != data_.end(); ++it) {
      const auto& s = it->second;
      for (auto iit = s.begin(); iit != s.end(); ++iit) {
        if (iit->first > to) {
          goto DYN_DATA_END;
        }
        if (iit->first > from) {
          data->emplace_back(iit->second);
        }
      }
    }
  DYN_DATA_END:
    return true;
  }

  bool put(uint64_t key, const std::shared_ptr<T>& data) override {
    std::unique_lock<std::mutex> lock(lock_);
    uint64_t slot = key / slot_size_;
    if (data_[slot].find(key) != data_[slot].end()) {
      LOG(WARNING) << "CachedData: Duplicate index: " << key;
      return false;
    }
    last_ = latest_;
    latest_ = key;
    data_[slot][key] = data;
    ++this->stat_.counter_add;
    return true;
  }
  bool put(uint64_t key, const T& data) override {
    std::shared_ptr<T> ptr = std::make_shared<T>(data);
    return put(key, ptr);
  }

  void remove_stale_data(const uint64_t& stale_time) override {
    std::unique_lock<std::mutex> lock(lock_);
    if (latest_ < stale_time) {
      return;
    }

    uint64_t k = 0;
    if (latest_ > stale_time) {
      k = (latest_ - stale_time) / slot_size_;
    }
    for (auto it = data_.begin(); it != data_.end();) {
      if (it->first < k) {
        ++this->stat_.counter_remove;
        it = data_.erase(it);
      } else {
        ++it;
      }
    }
  }

 protected:
  // 1s
  static const uint64_t slot_size_ = 1000000;

 private:
  template <typename U>
  friend class CachedData;

  size_t hz_;
  uint64_t last_;
  uint64_t latest_;
  mutable std::mutex lock_;
  std::map<uint64_t, std::map<uint64_t, std::shared_ptr<T>>> data_;
};

template <class T>
class StaticCachedData : public CachedDataBase<T> {
 public:
  explicit StaticCachedData(size_t hz)
      : CachedDataBase<T>(),
        hz_(hz),
        offset_(1e6 / hz),
        half_peroid_(5e5 / hz) {
    data_.clear();
  }

  virtual ~StaticCachedData() = default;

  size_t hz() const override { return hz_; }
  size_t uperiod() const override { return offset_; }

  std::string name() const override { return "StaticCachedData@" + std::to_string(hz_); }

  size_t size() const override {
    int size = 0;
    std::unique_lock<std::mutex> lock(lock_);
    for (auto& p : data_) {
      size += p.second.size();
    }
    return size * sizeof(T);
  }

  void reset() override {
    std::unique_lock<std::mutex> lock(lock_);
    data_.clear();
  }

  bool get(uint64_t key, std::shared_ptr<T>* data,
           int tolerate = FLAGS_cached_data_tolerate_offset) const override {
    std::unique_lock<std::mutex> lock(lock_);
    uint64_t slot = key / slot_size_;
    if (data_.find(slot) != data_.end() && data_.at(slot).find(key) != data_.at(slot).end()) {
      *data = data_.at(slot).at(key);
      return true;
    }

    bool found = false;
    if (tolerate > 0) {
      uint64_t tolerate_diff = offset_ * tolerate;
      uint64_t diff = std::numeric_limits<uint64_t>::max();
      for (uint64_t s = slot - 1; s < slot + 2; ++s) {
        if (data_.find(s) == data_.end()) {
          continue;
        }
        for (auto& p : data_.at(s)) {
          uint64_t dt = (key > p.first ? key - p.first : p.first - key);
          if (dt < diff && dt < tolerate_diff) {
            diff = dt;
            found = true;
            *data = data_.at(s).at(p.first);
          }
        }
      }
      this->stat_.counter_get += found;
    }
    return found;
  }

  bool get_newest(SharedPtr<const T>* data) const override {
    std::unique_lock<std::mutex> lock(lock_);
    uint64_t slot = latest_ / slot_size_;
    if (data_.find(slot) != data_.end() && data_.at(slot).find(latest_) != data_.at(slot).end()) {
      *data = data_.at(slot).at(latest_);
      ++this->stat_.counter_get;
      return true;
    }
    return false;
  }

  bool get(uint64_t from, uint64_t to, std::vector<SharedPtr<const T>>* data) const override {
    std::unique_lock<std::mutex> lock(lock_);
    if (data_.empty()) {
      LOG(WARNING) << this->key() << "CachedData: empty";
      return false;
    }

    for (auto it = data_.begin(); it != data_.end(); ++it) {
      const auto& s = it->second;
      for (auto iit = s.begin(); iit != s.end(); ++iit) {
        if (iit->first > to) {
          goto DYN_DATA_END;
        }
        if (iit->first > from) {
          data->emplace_back(iit->second);
        }
      }
    }
    DYN_DATA_END:
    return true;
  }

  bool put(uint64_t key, const std::shared_ptr<T>& data) override {
    std::unique_lock<std::mutex> lock(lock_);
    uint64_t slot = key / slot_size_;
    if (data_[slot].find(key) != data_[slot].end()) {
      LOG(WARNING) << "CachedData: Duplicate index: " << key;
      return false;
    }
    last_ = latest_;
    latest_ = key;
    data_[slot][key] = data;
    ++this->stat_.counter_add;
    return true;
  }

  bool put(uint64_t key, const T& data) override {
    std::shared_ptr<T> ptr = std::make_shared<T>(data);
    return put(key, ptr);
  }

  void remove_stale_data(const uint64_t& stale_time) override {
    std::unique_lock<std::mutex> lock(lock_);
    if (latest_ < stale_time) {
      return;
    }

    uint64_t k = 0;
    if (latest_ > stale_time) {
      k = (latest_ - stale_time) / slot_size_;
    }
    for (auto it = data_.begin(); it != data_.end();) {
      if (it->first < k) {
        ++this->stat_.counter_remove;
        it = data_.erase(it);
      } else {
        ++it;
      }
    }
  }

 protected:
  static const uint64_t slot_size_ = 1000000;

 private:
  template <typename U>
  friend class CachedData;

  size_t hz_;
  size_t offset_;
  size_t half_peroid_;
  uint64_t last_;
  uint64_t latest_;

  mutable std::mutex lock_;
  std::map<uint64_t, std::map<uint64_t, std::shared_ptr<T>>> data_;
  DISALLOW_COPY_AND_ASSIGN(StaticCachedData);
};

class FrameCachedData : public CachedData<Frame> {
 public:
  explicit FrameCachedData(size_t hz) : CachedData<Frame>(hz) {}
  virtual ~FrameCachedData() = default;

  std::string name() const override { return "FrameCachedData@" + std::to_string(hz()); }

 private:
  DISALLOW_COPY_AND_ASSIGN(FrameCachedData);
};

class ApplicationCachedData : public FrameCachedData {
 public:
  ApplicationCachedData() : FrameCachedData(10) {}
  virtual ~ApplicationCachedData() = default;

  std::string name() const override { return "ApplicationCachedData"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(ApplicationCachedData);
};

}  // namespace airi
}  // namespace crdc
