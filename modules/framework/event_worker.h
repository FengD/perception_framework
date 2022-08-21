// Copyright (C) 2021 Hirain Technologies
// License: Modified BSD Software License Agreement
// Author: Feng DING
// Description: Event Worker is the top input

#pragma once

#include <memory>
#include <string>
#include "common/common.h"
#include "framework/event.h"
#include "framework/event_manager.h"

namespace crdc {
namespace airi {

class Operator;
class EventWorker : public crdc::airi::common::Thread {
 public:
  // CV: condition_variable
  // EVENT: trigger
  // AUTO: CV or EVENT
  enum RunningMode : uint8_t {
    AUTO = 0,
    CV,
    EVENT,
  };
  EventWorker(std::shared_ptr<Operator> op, int idx);
  std::string name() const { return worker_name_; }
  WorkerID global_id() const;
  static WorkerID global_id(OperatorID op_id, int idx);
  virtual ~EventWorker();

 protected:
  bool set_running_mode(const RunningMode& mode) {
    if (!stop_) {
      return false;
    }
    mode_ = mode;
    return true;
  }
  void run() override;
  void stop();
  void peek_event();
  void proc_events();
  void process_cv();
  void process_operator();
  friend class Operator;

 private:
  std::shared_ptr<Operator> op_;
  std::string worker_name_ = "";
  EventMeta event_meta_;
  int idx_;
  bool stop_;
  bool inited_;
  size_t total_count_;
  size_t failed_count_;
  RunningMode mode_;
};

std::ostream& operator<<(std::ostream& os, const EventWorker& w);

}  // namespace airi
}  // namespace crdc
