// Copyright (C) 2021 FengD
// License: Modified BSD Software License Agreement
// Author: Feng DING
// Description: thread

#pragma once

#include <unistd.h>
#include <sys/syscall.h>
#include <sys/resource.h>
#include <pthread.h>
#include <mutex>
#include <string>
#include "common/common.h"

namespace crdc {
namespace airi {
namespace common {
class Thread {
 public:
  explicit Thread(bool joinable = false, const std::string& name = "Thread")
    : tid_(0), started_(false), joinable_(joinable), priority_(0), thread_name_(name) {}

  virtual ~Thread() = default;

  pthread_t tid() const {
    return tid_;
  }

  void set_joinable(bool joinable) {
      if (!started_) {
        joinable_ = joinable;
      }
  }

  void start();

  void join();

  bool is_alive();

  std::string get_thread_name() const {
    return thread_name_;
  }

  void set_thread_name(const std::string& name) {
    thread_name_ = name;
  }

  void set_priority(int priority);

  int get_priority() {
    return priority_;
  }

 protected:
  virtual void run() {}

  static void* thread_runner(void* arg) {
    Thread* t = reinterpret_cast<Thread*>(arg);
    setpriority(PRIO_PROCESS, syscall(SYS_gettid), t->get_priority());
    t->run();
    return NULL;
  }

  pthread_t tid_;
  bool started_;
  bool joinable_;
  int priority_;
  std::string thread_name_;
  std::mutex mutex_;

 private:
  DISALLOW_COPY_AND_ASSIGN(Thread);
};
}  // namespace common
}  // namespace airi
}  // namespace crdc
