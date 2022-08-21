// Copyright (C) 2021 Hirain Technologies
// License: Modified BSD Software License Agreement
// Author: Feng DING
// Description: Operator

#pragma once

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include "common/common.h"
#include "framework/cached_data.h"
#include "framework/event_manager.h"
#include "framework/event_worker.h"
#include "framework/shared_data_manager.h"
#include "framework/port.h"
#include "framework/processor.h"
#include "framework/proto/dag_config.pb.h"

namespace crdc {
namespace airi {

struct OperatorInfo {
  OperatorInfo() = default;
  bool is_running = false;
  uint64_t start_running_time = 0;
};

class OperatorInfoCachedData : public CachedData<OperatorInfo> {
 public:
  OperatorInfoCachedData() : CachedData<OperatorInfo>(0) {}
  virtual ~OperatorInfoCachedData() = default;

  std::string name() const override { return "OperatorInfoCachedData"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(OperatorInfoCachedData);
};

class Operator : public ParamManager, public std::enable_shared_from_this<Operator> {
 public:
  Operator();
  virtual ~Operator() = default;

  bool init(const OperatorID id, const OperatorConfig& config, EventManager* event_manager,
            SharedDataManager* shared_data_manager, const std::vector<EventMeta>& sub_events,
            const std::vector<std::vector<EventMeta>>& pub_events,
            std::map<std::string, std::string>& event_data_map);
  virtual void run();
  virtual void stop();
  void join();

  std::string name() const { return name_; }
  OperatorID id() const { return id_; }
  bool is_input() const { return is_input_; }

  virtual Status peek_event(int idx);
  virtual Status proc_events(int idx);

  void wait(int idx) {
    CHECK(is_input_);
    std::unique_lock<std::mutex> lock(*mutex_[idx].get());
    cv_[idx]->wait(lock);
  }
  void notify(int idx) {
    CHECK(is_input_);
    std::unique_lock<std::mutex> lock(*mutex_[idx].get());
    cv_[idx]->notify_one();
  }

  const std::vector<std::shared_ptr<EventWorker>>& workers() const { return workers_; }

  friend std::ostream& operator<<(std::ostream& os, const Operator& op);

 protected:
  bool set_running_mode(const EventWorker::RunningMode& mode) {
    if (is_input_ ^ (mode == EventWorker::RunningMode::EVENT)) {
      return true;
    }
    for (auto& w : workers_) {
      if (!w->set_running_mode(mode)) {
        return false;
      }
    }
    return true;
  }
  bool bypass();
  void reset_data(std::shared_ptr<Frame>* trigger,
                  std::vector<std::shared_ptr<const Frame>>* frames,
                  std::vector<std::shared_ptr<const Frame>>* latests) const;
  virtual void publish(int idx, std::shared_ptr<Frame>& trigger);
  virtual bool init_internal() { return true; }

  bool init_workers();

  bool init_group(OpGroupConfig& group);

  void update_info(int idx, bool running);

  void init_dependency_info();

  bool process_denpendency(std::shared_ptr<Frame>* trigger, bool& is_block);

  void process_denpendencies(std::shared_ptr<Frame>* trigger);

  void print_events();

  Status process_and_publish(int idx, bool is_peek);
  Status process_and_publish(int idx, std::shared_ptr<Frame>& trigger, bool is_peek);
  OpType type_;
  OperatorID id_;
  std::string name_;
  std::string algorithm_;

  std::shared_ptr<framework::Processor> processor_;

  OperatorConfig config_;
  EventManager* event_manager_ = nullptr;
  SharedDataManager* shared_data_manager_ = nullptr;
  std::vector<std::shared_ptr<EventMeta>> sub_meta_events_;
  std::vector<std::vector<EventMeta>> pub_meta_events_;

  std::vector<std::shared_ptr<EventWorker>> workers_;

  std::vector<Port> ports_;

  std::vector<std::string> input_data_name_;
  std::vector<std::string> input_event_name_;
  std::vector<std::string> latest_data_name_;
  std::vector<std::string> latest_event_name_;
  std::vector<std::string> trigger_data_name_;
  std::vector<std::string> trigger_event_name_;
  std::vector<std::string> output_event_name_;
  std::vector<std::vector<std::string>> output_data_name_;

  std::vector<std::string> perf_string_;

  std::vector<bool> is_running_;
  std::vector<uint64_t> start_time_;

  OperatorInfoCachedData* info_data_ = nullptr;
  std::vector<OperatorInfoCachedData *> deps_info_data_;
  std::vector<FrameCachedData *> deps_data_;

  size_t skip_latest_cnt_ = 0;

  volatile bool stop_;

 private:
  bool inited_ = false;
  bool is_input_ = false;
  volatile bool bypass_ = false;
  mutable std::vector<std::shared_ptr<std::mutex>> mutex_;
  mutable std::vector<std::shared_ptr<std::condition_variable>> cv_;

  DISALLOW_COPY_AND_ASSIGN(Operator);
};

std::ostream& operator<<(std::ostream& os, const Operator& op);

REGISTER_COMPONENT(Operator);

#define REGISTER_OPERATOR(name) REGISTER_CLASS(Operator, name)

}  // namespace airi
}  // namespace crdc
