// Copyright (C) 2021 FengD
// License: Modified BSD Software License Agreement
// Author: Feng DING
// Description: Event Worker

#include "framework/event_worker.h"
#include "framework/operator.h"

namespace crdc {
namespace airi {

std::ostream& operator<<(std::ostream& os, const EventWorker& w) {
  os << w.name();
  return os;
}

EventWorker::~EventWorker() {}

void EventWorker::run() {
  LOG(INFO) << "EventWorker[" << worker_name_ << "] starts.";
  stop_ = false;
  switch (mode_) {
    case RunningMode::AUTO:
      if (op_->is_input()) {
        return process_cv();
      }
      return process_operator();
    case RunningMode::CV:
      return process_cv();
    case RunningMode::EVENT:
      return process_operator();
  }
}

void EventWorker::stop() {
  stop_ = true;
  if (op_->is_input()) {
    op_->notify(idx_);
  }
}

void EventWorker::peek_event() {
  // peek event
  Status status = op_->peek_event(idx_);
  CHECK(status != Status::FATAL) << *op_ << ": peek event FATAL error, EXIT.";
  ++total_count_;
  if (status == Status::FAIL) {
    ++failed_count_;
    LOG(WARNING) << *op_ << " output [" << idx_ << "]: peek event failed. "
      << " total_count: " << total_count_ << " failed_count: " << failed_count_;
  }
}

inline void EventWorker::proc_events() {
  Status status = op_->proc_events(idx_);
  ++total_count_;
  if (status == Status::FAIL) {
    ++failed_count_;
    LOG(WARNING) << *op_ << " output [" << idx_ << "]: proc event failed. "
             << " total_count: " << total_count_ << " failed_count: " << failed_count_;
    return;
  }

  // FATAL error, so exit thread.
  CHECK(status != Status::FATAL) << *op_ << " output [" << idx_
                                 << "]: proc event FATAL error, EXIT. "
                                 << " total_count: " << total_count_
                                 << " failed_count: " << failed_count_;
}

void EventWorker::process_cv() {
  op_->wait(idx_);
  peek_event();
  while (!stop_) {
    op_->wait(idx_);
    if (stop_) {
      return;
    }
    proc_events();
  }
}

void EventWorker::process_operator() {
  peek_event();
  while (!stop_) {
    proc_events();
  }
}

EventWorker::EventWorker(std::shared_ptr<Operator> op, int idx)
    : crdc::airi::common::Thread(true),
      op_(op),
      idx_(idx),
      stop_(true),
      inited_(false),
      total_count_(0),
      failed_count_(0),
      mode_(RunningMode::AUTO),
      worker_name_(op->name()) {
  if (idx > 0) {
    worker_name_ += "[" + std::to_string(idx) + "]";
  }

  std::string thread_name = op->name();
  if (thread_name.length() > 13) {
    thread_name = thread_name.substr(0, 12);
  }
  set_thread_name(thread_name + "E" + std::to_string(idx));
  LOG(INFO) << "EventWorker::" << thread_name << " inited.";
}

WorkerID EventWorker::global_id(OperatorID op_id, int idx) {
  return (op_id << 15) + idx;
}

WorkerID EventWorker::global_id() const { return global_id(op_->id(), idx_); }

}  // namespace airi
}  // namespace crdc
