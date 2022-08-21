// Copyright (C) 2021 Hirain Technologies
// License: Modified BSD Software License Agreement
// Author: Feng DING
// Description: anlysis the dag file stream and create the app start thread

#include <framework/dag_streaming.h>
#include <unistd.h>
#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include "common/common.h"

namespace crdc {
namespace airi {

DAGStreaming::DAGStreaming()
    : crdc::airi::common::Thread(true, "DAGStreaming"),
    inited_(false), stop_(false) {}

DAGStreaming::~DAGStreaming() {}

bool DAGStreaming::init(const std::string& dag_config_path) {
  if (inited_) {
    LOG(WARNING) << "DAGStreaming init twice.";
    return true;
  }

  shared_data_manager_ = crdc::airi::common::Singleton<SharedDataManager>::get();
  DAGConfig dag_config;
  CHECK(crdc::airi::util::get_proto_from_file(dag_config_path, &dag_config))
      << "DAGStreaming failed to parse config prototxt:" << dag_config_path;
  if (!parse_config(dag_config)) {
    LOG(ERROR) << "Failed to parse dag config";
    return false;
  }

  if (!init_dag()) {
    LOG(ERROR) << "Failed to init dag";
    return false;
  }

  inited_ = true;
  stop_ = false;
  LOG(INFO) << "DAGStreaming init successfully";
  return true;
}

void DAGStreaming::schedule() {
  LOG(INFO) << "DAGStreaming start to schedule...";
  // start each operator with reverse sequence to avoid data lose
  for (auto it = ops_.rbegin(); it != ops_.rend(); ++it) {
    (*it)->run();
  }
  remove_stale_data();
  for (auto& op : ops_) {
    op->join();
    LOG(INFO) << "DAGStreaming: " << *op << " joined";
  }
  shared_data_manager_->reset();
  LOG(INFO) << "DAGStreaming schedule exit.";
}

void DAGStreaming::stop() {
  stop_ = true;
  for (auto& op : ops_) {
    LOG(WARNING) << "DAGStreaming: try to stop " << *op;
    op->stop();
  }
  LOG(WARNING) << "DAGStreaming: SharedDataManager stopped.";
  LOG(WARNING) << "DAGStreaming stopped.";
}

bool DAGStreaming::registe_data(std::string data_name, int hz, std::string data_type) {
  if (hz > 0) {
    if (!shared_data_manager_->register_frame_cached_data(data_name, hz)) {
      LOG(ERROR) << "Failed to register_frame_cached_data for [" << data_name << "], hz:<"
                 << hz << ">";
      return false;
    }
  } else {
    if (!shared_data_manager_->register_cached_data(data_name, data_type)) {
      LOG(ERROR) << "Failed to register_cached_data for [" << data_name << "], type:<" << data_type
             << ">";
      return false;
    }
  }
  FrameCachedData* data =
      dynamic_cast<FrameCachedData*>(shared_data_manager_->get_shared_data(data_name));
  LOG(INFO) << "Setup New CachedData `" << data_name << "` with HZ:" << data->hz();
  return true;
}

bool DAGStreaming::registe_data(const std::vector<OperatorConfig>& ops) {
  std::set<std::string> new_data;
  for (auto& op : ops) {
    LOG(INFO) << "REGISTER DATA For " << op.name() << " downstreams";
    for (int i = 0; i < op.output_size(); ++i) {
      auto& output = op.output(i);
      LOG(INFO) << op.name() << " output event:" << output.event() << " data:" << output.data()
            << " type:" << output.type() << " hz:" << output.hz()
            << " has_ref:" << output.has_reference();
      bool has_reference = output.has_reference();
      if (has_reference) {
        std::string output_data = output.event() + "_RO";
        int hz = -1;
        if (output.has_hz()) {
          hz = output.hz();
        }
        if (!registe_data(output_data, hz, output.type())) {
          return false;
        }
        if (!shared_data_manager_->register_data_event(output_data, output.event())) {
          return false;
        }
      }

      for (int j = 0; j < output.downstream_size(); ++j) {
        auto& downstream = output.downstream(j);
        auto output_data = downstream.data();
        if (new_data.count(output_data) > 0) {
          continue;
        }
        LOG(INFO) << "\tEvent:" << downstream.event() << " Name: " << downstream.data();
        int hz = -1;
        if (downstream.has_hz()) {
          hz = downstream.hz();
        }
        if (!registe_data(output_data, hz, downstream.type())) {
          return false;
        }
        new_data.insert(output_data);
      }
      auto data_name = output.data();
      if (new_data.count(data_name) > 0) {
        continue;
      }
      int hz = -1;
      if (output.has_hz()) {
        hz = output.hz();
      }
      LOG(INFO) << data_name << " " << output.type();
      if (!registe_data(data_name, hz, output.type())) {
        return false;
      }
      new_data.insert(data_name);
    }
  }
  return true;
}

bool DAGStreaming::parse_config(const DAGConfig& dag_config) {
  config_ = dag_config;
  config_.clear_op();
  std::vector<OperatorConfig> ops;
  if (!DAG::sort_operator(dag_config, &ops)) {
    return false;
  }

  ops_.resize(ops.size());
  for (size_t i = 0; i < ops.size(); ++i) {
    auto& op = ops[i];
    op.clear_trigger_data();
    for (int j = 0; j < op.trigger_size(); ++j) {
      op.mutable_trigger_data()->Add("");
    }

    std::string operator_type = "Operator";
    if (op.has_type()) {
      operator_type = op.type();
    }
    ops_[i] = OperatorFactory::get(operator_type);
    if (!ops_[i]) {
      LOG(ERROR) << "DAG: Failed to get <" << operator_type << ">";
      return false;
    }

    for (int j = 0; j < op.output_size(); ++j) {
      auto& output = op.output(j);
      if (output.has_hz() && output.has_type()) {
        LOG(ERROR) << op.name() << " output event: " << output.event()
               << " specified both output.type and output.hz";
        return false;
      }
    }
  }

  if (!DAG::link_operator(&ops)) {
    return false;
  }

  if (!DAG::set_reference(&ops)) {
    return false;
  }

  LOG(INFO) << "DAG SUMMARY:" << std::endl << DAG::summary(ops);

  if (!registe_data(ops)) {
    return false;
  }

  for (auto& op : ops) {
    OperatorConfig* o = config_.add_op();
    *o = op;
  }

  return true;
}

void DAGStreaming::run() {
  schedule();
}

void DAGStreaming::reset() {
  event_manager_->reset();
  shared_data_manager_->reset();
  LOG(INFO) << "DAGStreaming RESET.";
}

bool DAGStreaming::init_dag() {
  std::map<std::string, std::string> event_data_map;
  operator_pub_events_.resize(config_.op_size());
  operator_sub_events_.resize(config_.op_size());

  for (int i = 0; i < config_.op_size(); ++i) {
    auto& op = config_.op(i);
    operator_pub_events_[i].resize(op.output_size());
    if (op.upstream_size() > 0) {
      operator_sub_events_[i].resize(op.trigger_size());
    }
  }

  int event_id = 0;
  for (int i = 0; i < config_.op_size(); ++i) {
    auto& op = config_.op(i);
    OperatorID from_node = (OperatorID)op.id();
    for (int j = 0; j < op.output_size(); ++j) {
      event_data_map[op.output(j).event()] = op.output(j).data();
      LOG(INFO) << "EventDataMap: " << op.output(j).event() << " -> " << op.output(j).data();
      WorkerID from_worker = EventWorker::global_id(from_node, j);
      auto& op_output = op.output(j);
      if (op_output.downstream_size() == 0) {
        EventMeta meta;
        meta.from_node = from_worker;
        meta.to_node = from_worker;
        meta.event_id = ++event_id;
        std::ostringstream name_ss;
        name_ss << op.name() << "[" << j << "]_to_" << op.name() << "[ no downstream ]";
        meta.name = name_ss.str();
        events_.emplace_back(meta);
        LOG(INFO) << "Event:" << name_ss.str();
        operator_pub_events_[i][j].resize(1);
        operator_pub_events_[i][j][0] = meta;
        continue;
      }
      operator_pub_events_[i][j].resize(op_output.downstream_size());
      for (int k = 0; k < op_output.downstream_size(); ++k) {
        auto& downstream = op_output.downstream(k);
        auto& down_op = config_.op(downstream.op_id());
        int down_id = downstream.op_id();
        int down_trigger_id = downstream.trigger_id();
        WorkerID to_worker = EventWorker::global_id(down_id, down_trigger_id);
        EventMeta meta;
        meta.from_node = from_worker;
        meta.to_node = to_worker;
        meta.event_id = ++event_id;
        std::ostringstream name_ss;
        name_ss << op.name() << "[" << j << "]_to_" << down_op.name() << "[" << down_trigger_id
                << "]";
        meta.name = name_ss.str();
        events_.emplace_back(meta);

        LOG(INFO) << "Event:" << name_ss.str();

        operator_sub_events_[down_id][down_trigger_id] = meta;
        operator_pub_events_[i][j][k] = meta;
      }
    }
  }

  event_manager_ = crdc::airi::common::Singleton<EventManager>::get();
  if (!event_manager_->init(events_, config_)) {
    LOG(ERROR) << "failed to init EventManager.";
    return false;
  }

  for (int i = 0; i < config_.op_size(); ++i) {
    const auto& op = config_.op(i);
    auto& op_ptr = ops_[i];
    if (!op_ptr->init(i, op, event_manager_, shared_data_manager_, operator_sub_events_[i],
                      operator_pub_events_[i], event_data_map)) {
      LOG(ERROR) << "Failed to init Operator <" << op.name() << ">";
      return false;
    }
  }

  return true;
}

void DAGStreaming::remove_stale_data() {
  uint64_t n_usec = 500000;
  const auto dt = std::chrono::microseconds(n_usec);
  const uint64_t sleep_count = 1000000 / n_usec;

  while (!stop_) {
    if (FLAGS_enable_timing_remove_stale_data) {
      shared_data_manager_->remove_stale_data();
    }
    for (uint64_t c = 0; c < sleep_count; ++c) {
      if (stop_) {
        return;
      }
      std::this_thread::sleep_for(dt);
    }
  }
}

}  // namespace airi
}  // namespace crdc
