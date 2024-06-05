// Copyright (C) 2021 FengD
// License: Modified BSD Software License Agreement
// Author: Feng DING
// Description: Op is the minimum unit of execution. One operator could have
//              a single or multipule Ops. The op could be resumed or pause

#pragma once
#include <memory>
#include <ostream>
#include <string>
#include <vector>
#include "common/common.h"
#include "framework/frame.h"
#include "framework/param.h"

namespace crdc {
namespace airi {

class Op : public ParamManager {
 public:
  Op() = default;
  virtual ~Op() = default;

  virtual std::string name() const { return "NOT IMPLEMNENTED"; }
  virtual OpType type() const { return OpType::Processor; }

  /**
   * @brief init the inner members ( default do nothing )
   * @return true/false
   * @notes  implements either inited() or init(std::string&)
   */
  virtual bool inited() { return false; }

  /**
   * @brief init the inner members ( default do nothing )
   * @return true/false
   */
  virtual bool init(const std::string& config_path) { return false; }

  /**
   * @brief process for the first time
   * @return SUCC/FAIL/FATAL
   */
  virtual Status peek(int idx, const std::vector<std::shared_ptr<const Frame>>& frames,
                      std::shared_ptr<Frame>& data) {
    std::vector<std::shared_ptr<const Frame>> latests(latest_event_name_.size(), nullptr);
    return process(idx, frames, latests, data);
  }

  /**
   * @brief op process
   * @param[in] the input of op id
   * @param[in] all the input frame, the input frame is optional
   * @param[in&out] trigger frame
   * @return SUCC/FAIL/FATAL
   */
  virtual Status process(int idx, const std::vector<std::shared_ptr<const Frame>>& frames,
                         std::shared_ptr<Frame>& data) {
    return Status::FATAL;
  }

  /**
   * @brief op process
   * @param[in] the input of op id
   * @param[in] all the input frame, the input frame is optional
   * @param[in] all the latest frame, the latest frame is optional
   * @param[in&out] trigger frame
   * @return SUCC/FAIL/FATAL
   */
  virtual Status process(int idx, const std::vector<std::shared_ptr<const Frame>>& frames,
                         const std::vector<std::shared_ptr<const Frame>>& latests,
                         std::shared_ptr<Frame>& data) {
    return process(idx, frames, data);
  }

  /**
   * @brief could be used to do some bind
   * @param the name[std::string]
   */
  virtual void bind(const std::string& name) {}

  /**
   * @brief *only* used for peroidic op
   * @return SUCC/FAIL/FATAL
   */
  virtual Status callback(std::shared_ptr<Frame>* data) { return Status::FATAL; }

  virtual int min_input_num() const { return -1; }
  virtual int max_input_num() const { return -1; }
  virtual int input_num() const { return -1; }
  virtual int min_output_num() const { return -1; }
  virtual int max_output_num() const { return -1; }
  virtual int output_num() const { return -1; }
  virtual void stop() {}
  virtual void on_pause() {}
  virtual void on_resume() {}

  void set_event_io(const std::vector<std::string>& input,
                    const std::vector<std::string>& output,
                    const std::vector<std::string>& latest);

  void set_trigger(const std::vector<std::string>& data,
                   const std::vector<std::string>& event);

  friend std::ostream& operator<<(std::ostream& os, const Op& dt);

 protected:
  bool stop_;
  std::vector<std::string> trigger_data_name_;
  std::vector<std::string> input_event_name_;
  std::vector<std::string> output_event_name_;
  std::vector<std::string> trigger_event_name_;
  std::vector<std::string> latest_event_name_;
};

REGISTER_COMPONENT(Op);
#define REGISTER_OP(name) REGISTER_CLASS(Op, name)
}  // namespace airi
}  // namespace crdc
