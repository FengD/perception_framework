// Copyright (C) 2021 FengD
// License: Modified BSD Software License Agreement
// Author: Feng DING
// Description: concurrent queue

#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>
#include "common/common.h"

/** @file
 *  @ingroup THREAD
 *  @brief thread pool implementation
 */
namespace crdc {
namespace airi {
namespace common {
/**
 * @class ConcurrentQueue
 * @brief Thread-Safe Queue.
 * @param Data element type in container
 */
template <class Data>
class ConcurrentQueue {
 public:
  ConcurrentQueue() {}
  virtual ~ConcurrentQueue() {}

  /**
   * @brief inserts element at the end
   * @param data the value of the element to push
   * @note non-blocking
   */
  virtual void push(const Data& data) {
    std::unique_lock<std::mutex> lock(mutex_);
    queue_.push(data);
    cv_.notify_one();
  }

  /**
   * @brief access the first element
   * @return value of the first element
   * @note blocking if queue if ConcurrentQueue::empty
   */
  virtual Data front() const {
    std::unique_lock<std::mutex> lock(mutex_);

    cv_.wait(lock, [this] { return !queue_.empty(); });
    return queue_.front();
  }

  virtual void front(Data* data) const { *data = front(); }

  /**
   * @brief removes the first element
   * @note blocking if queue if empty
   */
  virtual void pop() {
    std::unique_lock<std::mutex> lock(mutex_);

    cv_.wait(lock, [this] { return !queue_.empty(); });
    queue_.pop();
  }

  /**
   * @brief removes the first element
   * @param[out] data value of the first element
   * @note blocking if queue if empty
   */
  virtual void pop(Data* data) {
    std::unique_lock<std::mutex> lock(mutex_);

    cv_.wait(lock, [this] { return !queue_.empty(); });
    *data = queue_.front();
    queue_.pop();
  }

  /**
   * @brief removes the first element if
   * @param[out] data value of the first element
   * @note non-blocking
   */
  virtual bool try_pop(Data* data) {
    std::unique_lock<std::mutex> lock(mutex_);

    if (queue_.empty()) {
      return false;
    }

    *data = queue_.front();
    queue_.pop();
    return true;
  }

  /**
   * @brief checks whether the underlying container is empty
   */
  bool empty() const {
    std::unique_lock<std::mutex> lock(mutex_);
    return queue_.empty();
  }

  /**
   * @brief returns the number of elements
   * @return the number of elements in the container
   */
  int size() const {
    std::unique_lock<std::mutex> lock(mutex_);
    return queue_.size();
  }

  /**
   * @brief remove all elements
   */
  void clear() {
    std::queue<Data> queue;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      queue_.swap(queue);
    }
  }

 protected:
  std::queue<Data> queue_;
  mutable std::mutex mutex_;
  mutable std::condition_variable cv_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConcurrentQueue);
};

/**
 * @class FixedSizeConQueue
 * @brief ConcurrentQueue with fixed size
 * @param Data element type
 */
template <typename Data>
class FixedSizeConQueue : public ConcurrentQueue<Data> {
 public:
  explicit FixedSizeConQueue(size_t max_count) : ConcurrentQueue<Data>(), max_count_(max_count) {}

  virtual ~FixedSizeConQueue() {}

  /**
   * @brief inserts element at the end
   * @param data the value of the element to push
   * @note blocking if the container is full
   */
  void push(const Data& data) override {
    std::unique_lock<std::mutex> lock(this->mutex_);
    cv_full_.wait(lock, [this] { return this->queue_.size() < max_count_; });
    this->queue_.push(data);
    this->cv_.notify_one();
  }

  virtual bool try_push(const Data& data) {
    std::unique_lock<std::mutex> lock(this->mutex_);
    if (this->queue_.size() < max_count_) {
      this->queue_.push(data);
      this->cv_.notify_one();
      return true;
    }
    return false;
  }

  void pop() override {
    std::unique_lock<std::mutex> lock(this->mutex_);

    this->cv_.wait(lock, [this] { return !this->queue_.empty(); });
    this->queue_.pop();
    cv_full_.notify_one();
  }

  void pop(Data* data) override {
    std::unique_lock<std::mutex> lock(this->mutex_);

    this->cv_.wait(lock, [this] { return !this->queue_.empty(); });
    *data = this->queue_.front();
    this->queue_.pop();
    cv_full_.notify_one();
  }

  bool try_pop(Data* data) override {
    std::unique_lock<std::mutex> lock(this->mutex_);

    if (this->queue_.empty()) {
      return false;
    }

    *data = this->queue_.front();
    this->queue_.pop();
    cv_full_.notify_one();
    return true;
  }

  /**
   * @brief checks whether the underlying container is full
   */
  bool full() const {
    std::unique_lock<std::mutex> lock(this->mutex_);
    return this->queue_.size() >= max_count_;
  }

 private:
  const size_t max_count_;
  mutable std::condition_variable cv_full_;
  DISALLOW_COPY_AND_ASSIGN(FixedSizeConQueue);
};

}  // namespace common
}  // namespace airi
}  // namespace crdc
