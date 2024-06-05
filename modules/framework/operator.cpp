// Copyright (C) 2021 FengD
// License: Modified BSD Software License Agreement
// Author: Feng DING
// Description: Operator

#include "framework/operator.h"

namespace crdc {
namespace airi {

DECLARE_int32(cached_data_tolerate_offset);

std::ostream& operator<<(std::ostream& os, const Operator& op) {
  os << "Operator[" << op.name_ << "]<" << op.algorithm_ << ">";
  return os;
}

Operator::Operator() : stop_(true) {}

bool Operator::init_workers() {
  int worker_size = config_.trigger_size();
  workers_.resize(worker_size);
  for (int i = 0; i < worker_size; ++i) {
    workers_[i].reset(new EventWorker(shared_from_this(), i));
    if (config_.has_priority()) {
      workers_[i]->set_priority(config_.priority());
      LOG(INFO) << "Operator: " << name_ << ", id: " << i
            << ", priority: " << workers_[i]->get_priority();
    }
  }
  is_running_.assign(worker_size, false);
  start_time_.assign(worker_size, 0);

  return true;
}

static bool to_group_config(const OperatorConfig& config, OpGroupConfig* group) {
  if (!config.has_algorithm()) {
    return false;
  }
  auto op = group->add_op();
  op->set_algorithm(config.algorithm());

  for (int i = 0; i < config.param_size(); ++i) {
    *(op->add_param()) = config.param(i);
  }

  if (config.has_config()) {
    op->set_config(config.config());
  }
  if (config.has_bypass()) {
    op->set_bypass(config.bypass());
  }
  return true;
}

bool Operator::init_group(OpGroupConfig& group) {
  switch (config_.op_case()) {
    case OperatorConfig::kAlgorithm:
      algorithm_ = config_.algorithm();
      if (!to_group_config(config_, &group)) {
        return false;
      }
      break;
    case OperatorConfig::kGroup:
      group = config_.group();
      if (group.op_size() == 0) {
        LOG(ERROR) << *this << " has no op in group";
        return false;
      }
      algorithm_ = group.op(0).algorithm();
      break;
    case OperatorConfig::OP_NOT_SET:
      LOG(ERROR) << *this << " config should specify oneof `algorithm` or `group`";
      return false;
  }

  for (int i = 0; i < group.op_size(); ++i) {
    auto op = group.mutable_op(i);
    if (op->has_config()) {
      op->set_config(std::string(std::getenv("CRDC_WS")) + "/" + op->config());
    }
  }
  return true;
}

void Operator::print_events() {
  for (auto& sub : sub_meta_events_) {
    if (sub) {
      LOG(INFO) << *this << ": subscribe to " << sub->name;
    }
  }
  for (size_t k = 0; k < pub_meta_events_.size(); ++k) {
    for (auto& p : pub_meta_events_[k]) {
      LOG(INFO) << *this << " Worker[" << k << "]: publish to " << p.name;
    }
  }
}

bool Operator::init(const OperatorID id, const OperatorConfig& config, EventManager* event_manager,
                    SharedDataManager* shared_data_manager,
                    const std::vector<EventMeta>& sub_events,
                    const std::vector<std::vector<EventMeta>>& pub_events,
                    std::map<std::string, std::string>& event_data_map) {
  id_ = id;
  config_ = config;
  name_ = config_.name();
  bypass_ = config_.bypass();
  CHECK(event_manager) << "event_manager == NULL";
  event_manager_ = event_manager;

  CHECK(shared_data_manager) << "shared_data_manager == NULL";
  shared_data_manager_ = shared_data_manager;

  OpGroupConfig group;
  if (!init_group(group)) {
    LOG(ERROR) << *this << " Failed to init group";
    return false;
  }

  if (!init_params(config_.param())) {
    LOG(ERROR) << *this << " Failed to init params";
    return false;
  }

  processor_.reset(new framework::SeqProcessor);

  CHECK_GT(config_.trigger_size(), 0);
  cv_.resize(config_.trigger_size());
  mutex_.resize(config_.trigger_size());
  for (int i = 0; i < config_.trigger_size(); ++i) {
    cv_[i].reset(new std::condition_variable);
    mutex_[i].reset(new std::mutex);
  }
  output_data_name_.resize(config_.trigger_size());
  output_event_name_.resize(config_.trigger_size());
  trigger_data_name_.resize(config_.trigger_size());
  trigger_event_name_.resize(config_.trigger_size());

  is_input_ = sub_events.empty();
  if (config_.trigger_size() < static_cast<int>(sub_events.size())) {
    LOG(ERROR) << *this << " trigger size < sub_events size: " << config_.trigger_size() << " vs. "
           << sub_events.size();
    return false;
  }
  sub_meta_events_.assign(config_.trigger_size(), nullptr);
  for (size_t i = 0; i < sub_events.size(); ++i) {
    sub_meta_events_[i].reset(new EventMeta(sub_events[i]));
  }
  pub_meta_events_ = pub_events;

  perf_string_.resize(config_.trigger_size());
  ports_.resize(config_.trigger_size());
  for (int i = 0; i < config_.trigger_size(); ++i) {
    std::vector<EventMeta> pub_events_c;
    if (static_cast<int>(pub_events.size()) > i) {
      for (size_t j = 0; j < pub_events[i].size(); ++j) {
        pub_events_c.emplace_back(pub_events[i][j]);
      }
    }
    if (!ports_[i].init(i, config, event_manager, shared_data_manager, sub_meta_events_[i],
                        pub_events_c, event_data_map)) {
      return false;
    }
    trigger_data_name_[i] = ports_[i].trigger_data_name();
    trigger_event_name_[i] = ports_[i].trigger_event_name();
    output_data_name_[i] = ports_[i].output_data_name();
    output_event_name_[i] = ports_[i].output_event_name();
    perf_string_[i] = "Operator<" + algorithm_ + "> get_input_data [" + std::to_string(i) + "]";
  }
  input_data_name_ = ports_[0].input_data_name();
  input_event_name_ = ports_[0].input_event_name();
  latest_data_name_ = ports_[0].latest_data_name();
  latest_event_name_ = ports_[0].latest_event_name();

  processor_->set_input_event_name(input_event_name_);
  processor_->set_output_event_name(output_event_name_);
  processor_->set_latest_event_name(latest_event_name_);
  processor_->set_trigger_event_name(trigger_event_name_);
  processor_->set_trigger_data_name(trigger_data_name_);

  if (!processor_->init(group)) {
    return false;
  }

  if (!init_workers()) {
    return false;
  }

  if (!init_internal()) {
    LOG(ERROR) << "failed to Operator::init_internal";
    return false;
  }

  print_events();

  if (!shared_data_manager_->register_cached_data(name_, "OperatorInfoCachedData")) {
    LOG(ERROR) << "Failed to register OperatorInfoCachedData for [" << name_ << "]";
    return false;
  }
  info_data_ =
      dynamic_cast<OperatorInfoCachedData*>(shared_data_manager_->get_shared_data(name_));

  inited_ = true;
  stop_ = false;
  return true;
}

void Operator::join() {
  for (auto& worker : workers_) {
    if (worker->is_alive()) {
      worker->join();
    }
    LOG(INFO) << "EventWorker[" << worker->name() << "] joined successfully";
  }
  LOG(INFO) << *this << " work complete";
}

void Operator::stop() {
  stop_ = true;
  for (auto& w : workers_) {
    w->stop();
  }
  for (auto& w : workers_) {
    LOG(WARNING) << "DAGStreaming: Check worker is stopped: " << *w << " ...";
    if (w->is_alive()) {
      if (pthread_cancel(w->tid()) != 0) {
        LOG(ERROR) << *w << " Failed to Kill!";
      }
    }
  }
  processor_->stop();
  for (auto sub : sub_meta_events_) {
    if (!sub) {
      continue;
    }
    Event event;
    event.event_id = sub->event_id;
    event.timestamp = 0;
    event.reserve = "";
    event_manager_->publish(event);
  }
}

bool Operator::bypass() {
  if (bypass_) {
    LOG(INFO) << *this << " BYPASSED";
    return true;
  }
  return false;
}

void Operator::init_dependency_info() {
  deps_info_data_.resize((config_.dependency()).size());
  for (int i = 0; i < (config_.dependency()).size(); ++i) {
    auto& op_dep = config_.dependency(i);
    LOG(INFO) << "op_dep.name: " << op_dep.name();
    deps_info_data_.at(i) = (dynamic_cast<OperatorInfoCachedData*>(
                                        shared_data_manager_->get_shared_data(op_dep.name())));
    if (!deps_info_data_[i]) {
      LOG(WARNING) << "Failed to get dep info: " << op_dep.name();
    }
  }
}

bool Operator::process_denpendency(std::shared_ptr<Frame>* trigger, bool& is_block) {
  uint64_t wait_time = 0;
  uint64_t now = get_now_microsecond();
  is_block = true;
  for (size_t i = 0; i < deps_info_data_.size(); ++i) {
    if (deps_info_data_[i]) {
      std::shared_ptr<const OperatorInfo> info = nullptr;
      if (deps_info_data_[i]->get_newest(&info) && info) {
        auto& op_dep = config_.dependency(i);
        uint64_t op_wait_time = (uint64_t)op_dep.wait_time() * 1000;
        int64_t time_diff;
        switch (op_dep.policy()) {
          case OperatorDependency_DependencyPolicy_WAIT:
            if (info->is_running && (info->start_running_time + op_wait_time > now)) {
              wait_time = std::max(info->start_running_time + op_wait_time - now, (uint64_t)1000);
            }
            break;
          case OperatorDependency_DependencyPolicy_BLOCK:
            if (info->is_running) {
              wait_time = std::max(op_wait_time, (uint64_t)5000);
              is_block = true;
            }
            break;
          case OperatorDependency_DependencyPolicy_BUNDLE:
            time_diff = (int64_t)now - (int64_t)(*trigger)->base_frame->utime;
            if (std::abs(time_diff) < static_cast<int64_t>(op_wait_time)) {
              wait_time = std::max(op_wait_time - time_diff, (uint64_t)1000);
            }
            break;
          default:
            LOG(ERROR) << "Now not support policy: "
                  << OperatorDependency_DependencyPolicy_Name(op_dep.policy());
            break;
        }
      }
    }
  }
  if (wait_time > 0) {
    LOG(INFO) << "process_denpendency " << name_ << " wait: " << wait_time << " us";
    std::this_thread::sleep_for(std::chrono::microseconds(wait_time));
  }
  return wait_time > 0;
}

void Operator::process_denpendencies(std::shared_ptr<Frame>* trigger) {
  bool retry = true;
  int process_cnt = 0;
  uint64_t now = get_now_microsecond();
  while (retry) {
    bool is_block;
    retry = false;
    if (process_denpendency(trigger, is_block)) {
      if (is_block) {
        retry = static_cast<bool>(get_now_microsecond() - now <
                                    100000);
      } else {
        retry = static_cast<bool>(process_cnt < 1);
      }
    }
    ++process_cnt;
  }
}

void Operator::update_info(int idx, bool running) {
  uint64_t now = get_now_microsecond();
  is_running_[idx] = running;
  if (running) {
    start_time_[idx] = now;
  }

  std::shared_ptr<OperatorInfo> info = std::make_shared<OperatorInfo>();
  if (!inited_ || bypass()) {
    info->is_running = false;
  } else {
    info->start_running_time = now;
    for (size_t i = 0; i < is_running_.size(); i++) {
      if (is_running_[i]) {
        info->start_running_time = std::min(info->start_running_time, start_time_[i]);
        info->is_running = true;
      }
    }
  }
  if (!info_data_->put(now, info)) {
    LOG(ERROR) << "Fail to put data: " << name_;
  }
}

Status Operator::process_and_publish(int idx, bool is_peek) {
  if (!sub_meta_events_[idx]) {
    return Status::FAIL;
  }
  std::shared_ptr<Frame> trigger;
  auto& port = ports_[idx];
  Event sub_event;
  if (!port.get_trigger_data(&trigger, &sub_event)) {
    return Status::FAIL;
  }
  CHECK(trigger);
  return process_and_publish(idx, trigger, is_peek);
}

Status Operator::process_and_publish(int idx, std::shared_ptr<Frame>& trigger, bool is_peek) {
  std::vector<std::shared_ptr<const Frame>> frames;
  std::vector<std::shared_ptr<const Frame>> latests;

  auto& port = ports_[idx];
  Status ret = Status::SUCC;
  if (!bypass()) {
    process_denpendencies(&trigger);
    update_info(idx, true);
    if (!port.get_input_data(trigger->base_frame->utime, &frames)) {
      ret = Status::FAIL;
    } else if (is_peek) {
      ret = processor_->peek(idx, frames, trigger);
      latests.resize(port.latest_event_name().size(), nullptr);
    } else {
      if (!port.get_latest_data(trigger->base_frame->utime, &latests)) {
        LOG(ERROR) << *this << " Failed to get latests data";
      }
      ret = processor_->process(idx, frames, latests, trigger);
    }
    update_info(idx, false);
  }

  if (ret == Status::SUCC || ret == Status::IGNORE) {
    port.publish(trigger);
  }

  return ret;
}

Status Operator::peek_event(int idx) {
  LOG(INFO) << *this << " peek event";
  return process_and_publish(idx, true);
}

Status Operator::proc_events(int idx) { return process_and_publish(idx, false); }

void Operator::reset_data(std::shared_ptr<Frame>* trigger,
                          std::vector<std::shared_ptr<const Frame>>* frames,
                          std::vector<std::shared_ptr<const Frame>>* latest) const {
  for (auto& f : *frames) {
    f.reset();
  }
  for (auto& r : *latest) {
    r.reset();
  }
  trigger->reset();
}

void Operator::run() {
  if (!inited_) {
    LOG(ERROR) << *this << " not initialized, run failed";
    return;
  }

  init_dependency_info();

  for (auto& worker : workers_) {
    worker->start();
  }
}

void Operator::publish(int idx, std::shared_ptr<Frame>& trigger) { ports_[idx].publish(trigger); }

REGISTER_OPERATOR(Operator);
REGISTER_SHARED_DATA(OperatorInfoCachedData);
}  // namespace airi
}  // namespace crdc
