// Copyright (C) 2021 Hirain Technologies
// License: Modified BSD Software License Agreement
// Author: Feng DING
// Description: thread_safe_queue

#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>

namespace crdc {
namespace airi {
namespace common {
template <typename T>
class ThreadSafeQueue {
 public:
  ThreadSafeQueue() {}
  ThreadSafeQueue &operator=(const ThreadSafeQueue &other) = delete;
  ThreadSafeQueue(const ThreadSafeQueue &other) = delete;
  ~ThreadSafeQueue() { break_all_wait(); }

  void enqueue(const T &element) {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.emplace(element);
    cv_.notify_one();
  }

  bool dequeue(T *element) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) {
      return false;
    }
    *element = std::move(queue_.front());
    queue_.pop();
    return true;
  }

  bool wait_dequeue(T *element) {
    std::lock_guard<std::mutex> lock(mutex_);
    cv_.wait(lock, [this]() { return break_all_wait_ || !queue_.empty(); });
    if (break_all_wait_) {
      return false;
    }
    *element = std::move(queue_.front());
    queue_.pop();
    return true;
  }

  bool wait_for_dequeue(T *element, const uint32_t& usec = 500000) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!cv_.wait_for(lock, std::chrono::microseconds(usec), [this]() {
      return break_all_wait_ || !queue_.empty();
    })) {
      return false;
    }

    if (break_all_wait_) {
      return false;
    }

    *element = std::move(queue_.front());
    queue_.pop();
    return true;
  }

  typename std::queue<T>::size_type size() {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
  }

  bool empty() {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
  }

  void break_all_wait() {
    break_all_wait_ = true;
    cv_.notify_all();
  }

  void clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!queue_.empty()) {
      queue_.pop();
    }
  }

  void reset() { break_all_wait = false; }

 private:
  volatile bool break_all_wait_ = false;
  std::mutex mutex_;
  std::queue<T> queue_;
  std::condition_variable cv_;
};
}  // namespace common
}  // namespace airi
}  // namespace crdc
