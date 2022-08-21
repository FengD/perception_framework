// Copyright (C) 2021 Hirain Technologies
// License: Modified BSD Software License Agreement
// Author: Feng DING
// Description: Port is used to connect the data pipeline for each op.

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
#include "framework/shared_data_manager.h"
#include "framework/proto/dag_config.pb.h"

namespace crdc {
namespace airi {

class Port {
 public:
  Port();

  /**
   * @brief init the port
   */
  bool init(const size_t idx, const OperatorConfig& config, EventManager* event_manager,
            SharedDataManager* shared_data_manager,
            const std::shared_ptr<const EventMeta>& sub_event,
            const std::vector<EventMeta>& pub_events,
            const std::map<std::string, std::string>& event_data_map);

  /**
   * @brief getter of all the variable names
   */
  std::string name() const { return name_; }
  const std::string& trigger_data_name() const { return trigger_data_name_; }
  const std::string& trigger_event_name() const { return trigger_event_name_; }
  const std::vector<std::string>& input_data_name() const { return input_data_name_; }
  const std::vector<std::string>& input_event_name() const { return input_event_name_; }
  const std::vector<std::string>& output_data_name() const { return output_data_name_; }
  const std::string& output_event_name() const { return output_event_name_; }
  const std::vector<std::string>& latest_data_name() const { return latest_data_name_; }
  const std::vector<std::string>& latest_event_name() const { return latest_event_name_; }

  /**
   * @brief data getter of each type data
   */
  bool get_trigger_data(std::shared_ptr<Frame>* trigger, Event* sub_event);
  bool get_latest_data(uint64_t timestamp, std::vector<std::shared_ptr<const Frame>>* latests);
  bool get_latest_data(const std::vector<uint64_t>& timestamp,
                            std::vector<std::shared_ptr<const Frame>>* latests);
  bool get_input_data(uint64_t timestamp, std::vector<std::shared_ptr<const Frame>>* frames);
  bool get_input_data(const std::vector<uint64_t>& timestamp,
                            std::vector<std::shared_ptr<const Frame>>* frames);

  /**
   * @brief publish the trigger Frame. Gives the timestamp, footprint of the data.
   * @param trigger Frame
   */
  void publish(const std::shared_ptr<Frame>& trigger);

  friend std::ostream& operator<<(std::ostream& os, const Port& port);

 protected:
  /**
   * @brief init all type of data.
   *        input, output, trigger, latest
   */
  bool init_trigger_data();
  bool init_output_data();
  bool init_input_data(const std::map<std::string, std::string>& event_data_map);
  bool init_latest_data(const std::map<std::string, std::string>& event_data_map);

  size_t idx_ = 0;
  bool is_input_ = false;
  std::string name_;
  OperatorConfig config_;

  bool has_reference_ = false;
  std::string ref_data_name_;
  FrameCachedData* ref_data_ = nullptr;

  EventManager* event_manager_ = nullptr;
  SharedDataManager* shared_data_manager_ = nullptr;

  std::shared_ptr<EventMeta> sub_meta_event_ = nullptr;
  std::vector<EventMeta> pub_meta_events_;

  // trigger data variable
  std::string trigger_data_name_;
  std::string trigger_event_name_;
  FrameCachedData* trigger_data_ = nullptr;

  // output data variable
  bool has_downstream_ = false;
  std::string output_event_name_;
  std::vector<std::string> output_data_name_;
  std::vector<FrameCachedData*> output_data_;
  std::vector<unsigned int> output_period_;
  std::vector<uint64_t> output_last_;
  std::vector<int> output_copy_idx_;
  std::vector<int> output_nocopy_idx_;

  // input data variable
  std::vector<int> input_wait_;
  std::vector<int> input_window_;
  std::vector<uint64_t> input_offset_;
  std::vector<std::string> input_data_name_;
  std::vector<std::string> input_event_name_;
  std::vector<FrameCachedData*> input_data_;

  // latest name variable
  std::vector<std::string> latest_data_name_;
  std::vector<std::string> latest_event_name_;
  std::vector<FrameCachedData*> latest_data_;
  std::vector<int> latest_tolerate_offset_;

  uint64_t last_ts_ = 0;
};

std::ostream& operator<<(std::ostream& os, const Port& port);

}  // namespace airi
}  // namespace crdc
