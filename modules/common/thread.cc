// Copyright (C) 2021 FengD
// License: Modified BSD Software License Agreement
// Author: Feng DING
// Description: thread


#include <signal.h>
#include <glog/logging.h>
#include "common/thread.h"

namespace crdc {
namespace airi {
namespace common {
void Thread::start() {
  std::lock_guard<std::mutex> lock(mutex_);
  CHECK(!started_) << thread_name_ << " start twice";
  pthread_attr_t attr;
  CHECK_EQ(pthread_attr_init(&attr), 0);
  CHECK_EQ(pthread_attr_setdetachstate(
      &attr, joinable_ ? PTHREAD_CREATE_JOINABLE : PTHREAD_CREATE_DETACHED), 0);
  CHECK_EQ(pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL), 0);
  CHECK_EQ(pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL), 0);
  int result = pthread_create(&tid_, &attr, &thread_runner, this);
  CHECK_EQ(result, 0) << "Could not create thread (" << result << ")";
  result = pthread_setname_np(tid_, thread_name_.c_str());
  if (result != 0) {
    LOG(INFO) << "Could not set thread name:" << thread_name_;
  }

  CHECK_EQ(pthread_attr_destroy(&attr), 0);
  started_ = true;
}

void Thread::join() {
  CHECK(joinable_) << "Thread is not joinable";
  int result = pthread_join(tid_, NULL);
  CHECK_EQ(result, 0) << "Cloud not join thread (" << tid_ << ", " << thread_name_ << ")";
  std::lock_guard<std::mutex> lock(mutex_);
  tid_ = 0;
}

bool Thread::is_alive() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (tid_ == 0) {
    return false;
  }
  // no signal sent, just check existence for thread
  int ret = pthread_kill(tid_, 0);

  if (ret = ESRCH) {
    return false;
  }

  if (ret = EINVAL) {
    LOG(WARNING) << "Invalid signal!";
    return false;
  }

  return true;
}

void Thread::set_priority(int priority) {
  if (priority > 19) {
    priority = 19;
  } else if (priority < -20) {
    priority = -20;
  }
  priority_ = priority;
}
}  // namespace common
}  // namespace airi
}  // namespace crdc
