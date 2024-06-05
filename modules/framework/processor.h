// Copyright (C) 2021 FengD
// License: Modified BSD Software License Agreement
// Author: Feng DING
// Description: Processer is used to execute all the Op and it is executed by Operator.

#pragma once

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "framework/op.h"
#include "framework/cached_data.h"
#include "framework/proto/dag_config.pb.h"

namespace crdc {
namespace airi {

namespace framework {

class Processor {
 public:
  Processor() = default;

  /**
   * @brief stop all the op
   */
  void stop() {
    for (auto& op : ops_) {
      if (op) {
        op->stop();
      }
    }
  }

  /**
   * @brief getter and setter of processer name
   */
  const std::string& name() const { return name_; }
  void set_name(const std::string& name) { name_ = name; }

  /**
   * @brief set input event names
   */
  void set_input_event_name(const std::vector<std::string>& input_event_name) {
    input_event_name_ = input_event_name;
  }

  /**
   * @brief set output event names
   */
  void set_output_event_name(const std::vector<std::string>& output_event_name) {
    output_event_name_ = output_event_name;
  }

  /**
   * @brief set latest input event names
   */
  void set_latest_event_name(const std::vector<std::string>& latest_event_name) {
    latest_event_name_ = latest_event_name;
  }

  /**
   * @brief set trigger event names
   */
  void set_trigger_event_name(const std::vector<std::string>& trigger_event_name) {
    trigger_event_name_ = trigger_event_name;
  }

  /**
   * @brief set trigger data names
   */
  void set_trigger_data_name(const std::vector<std::string>& trigger_data_name) {
    trigger_data_name_ = trigger_data_name;
  }

  /**
   * @brief init the op group, if the io is not correct or the op is bypassed,
   *        it will not be executed.
   * @param the config of op group
   * @return if the init success
   */
  virtual bool init(const OpGroupConfig& group) = 0;

  /**
   * @brief execute once
   * @param[in] the id of the process
   * @param[in] the input frame of the process[optional]
   * @param[in] the trigger frame of the process
   * @return the status of the action[Status]
   */
  virtual Status peek(const int& idx, const std::vector<std::shared_ptr<const Frame>>& frames,
                      std::shared_ptr<Frame>& data) = 0;

  /**
   * @brief execute once
   * @param[in] the id of the process
   * @param[in] the input frame of the process[optional]
   * @param[in] the latests input frame of the process[optional]
   * @param[in] the trigger frame of the process
   * @return the status of the action[Status]
   */
  virtual Status process(const int& idx, const std::vector<std::shared_ptr<const Frame>>& frames,
                         const std::vector<std::shared_ptr<const Frame>>& latests,
                         std::shared_ptr<Frame>& data) = 0;

 protected:
  /**
   * @brief init op
   */
  bool init_op(const OpConfig& config, std::shared_ptr<Op>* o);

  /**
   * @brief check if the op is ok, beacuse normally the op could have input,
   *        output, latest and trigger. It is to check if the input and output
   *        number is correct.
   */
  bool io_sanity_check(const std::shared_ptr<Op>& in, const std::shared_ptr<Op>& out);

  /**
   * @brief init the perfermence in string
   */
  void init_perf_string(const OpGroupConfig& config);

  std::vector<std::string> input_event_name_;
  std::vector<std::string> output_event_name_;
  std::vector<std::string> latest_event_name_;
  std::vector<std::string> trigger_event_name_;
  std::vector<std::string> trigger_data_name_;

  std::string name_;
  std::vector<OpConfig> configs_;
  std::vector<std::shared_ptr<Op>> ops_;
  std::vector<std::vector<std::string>> perf_string_;
};

std::ostream& operator<<(std::ostream& os, const Processor& p);

/**
 * @brief the Processor which execute all the Op in sequence
 */
class SeqProcessor : public Processor {
 public:
  bool init(const OpGroupConfig& group) override;
  Status peek(const int& idx, const std::vector<std::shared_ptr<const Frame>>& frames,
              std::shared_ptr<Frame>& data) override;
  Status process(const int& idx, const std::vector<std::shared_ptr<const Frame>>& frames,
                 const std::vector<std::shared_ptr<const Frame>>& latests,
                 std::shared_ptr<Frame>& data) override;

 private:
  bool ignore_fail_ = false;
  OpGroupConfig config_;
  std::vector<int> valid_;
};

}  // namespace framework
}  // namespace airi
}  // namespace crdc
