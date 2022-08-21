// Copyright (C) 2021 Hirain Technologies
// License: Modified BSD Software License Agreement
// Author: Feng DING
// Description: Processer is used to execute all the Op and it is executed by Operator.

#include <algorithm>
#include "framework/processor.h"

namespace crdc {
namespace airi {
namespace framework {

std::ostream& operator<<(std::ostream& os, const Processor& p) {
  os << p.name();
  return os;
}

bool Processor::io_sanity_check(const std::shared_ptr<Op>& in, const std::shared_ptr<Op>& out) {
  // check if the Op has the correct num of input itself
  auto check = [](const std::string& name, int num, int min, int max) {
    if (min > 0 && max > 0) {
      CHECK_LT(min, max) << name;
    }
    if (num > 0) {
      if (max > 0) {
        CHECK_LE(num, max) << name;
      }
      if (min > 0) {
        CHECK_GE(num, min) << name;
      }
    }
    return true;
  };
  if (!check(in->name(), in->input_num(), in->min_input_num(), in->max_input_num())) {
    return false;
  }
  if (!check(out->name(), out->output_num(), out->min_output_num(), out->max_output_num())) {
    return false;
  }

  // check if the event input number is correct
  auto check_num = [&](std::string io, int num, int min_num, int max_num, int size) {
    if (num > 0 && num != size) {
      LOG(ERROR) << *this << ": " << io << " size:" << size << " != " << num;
      return false;
    } else {
      if (min_num > 0 && min_num > size) {
        LOG(ERROR) << *this << ": " << io << " size:" << size << " < " << min_num;
        return false;
      }
      if (max_num > 0 && max_num < size) {
        LOG(ERROR) << *this << ": " << io << " size:" << size << " > " << max_num;
        return false;
      }
    }
    return true;
  };

  if (!check_num("input", in->input_num(), in->min_input_num(), in->max_input_num(),
                 input_event_name_.size())) {
    return false;
  }

  if (!check_num("output", out->output_num(), out->min_output_num(), out->max_output_num(),
                 output_event_name_.size())) {
    return false;
  }
  return true;
}

bool Processor::init_op(const OpConfig& config, std::shared_ptr<Op>* o) {
  auto start_ts = get_now_microsecond();
  auto& op = *o;
  if (!op) {
    op = OpFactory::get(config.algorithm());
  }

  if (!op->init_params(config.param())) {
    LOG(ERROR) << "Failed to init params for " << *this;
    return false;
  }

  op->set_event_io(input_event_name_, output_event_name_, latest_event_name_);
  op->set_trigger(trigger_data_name_, trigger_event_name_);

  LOG(INFO) << *this << " initialize " << *op << " ...";
  if (!op->inited()) {
    if (!config.has_config()) {
      LOG(ERROR) << "Fail to init Op[" << config.algorithm() << "] without config file";
      return false;
    }
    auto config_file = config.config();
    if (!op->init(config_file)) {
      LOG(ERROR) << "Fail to init Op[" << config.algorithm()
                 << "] from config file " << config_file;
      return false;
    }
    LOG(INFO) << *this << " Op[" << config.algorithm() << "] initialized from config file:["
              << config_file << "]";
  }

  std::stringstream ss;
  ss << *this << " initialization";
  LOG(INFO) << ss.str() << " elapsed_time: "
            << get_now_microsecond() - start_ts << " us";
  return true;
}

void Processor::init_perf_string(const OpGroupConfig& config) {
  perf_string_.resize(trigger_event_name_.size());
  for (size_t i = 0; i < trigger_event_name_.size(); ++i) {
    perf_string_[i].resize(ops_.size());
    for (size_t j = 0; j < ops_.size(); ++j) {
      perf_string_[i][j] = config.op(j).algorithm();
    }
  }
}

bool SeqProcessor::init(const OpGroupConfig& group) {
  config_ = group;
  if (config_.op_size() == 0) {
    return false;
  }
  name_ = config_.op(0).algorithm();
  ops_.resize(config_.op_size());
  for (int i = 0; i < config_.op_size(); ++i) {
    auto alg = config_.op(i).algorithm();
    ops_[i] = OpFactory::get(alg);
    if (!ops_[i]) {
      return false;
    }
  }
  if (!io_sanity_check(ops_.front(), ops_.back())) {
    return false;
  }
  ignore_fail_ = config_.seq_config().ignore_fail();
  for (int i = 0; i < config_.op_size(); ++i) {
    if (config_.op(i).bypass()) {
      continue;
    }
    valid_.emplace_back(i);
    if (!init_op(config_.op(i), &ops_[i])) {
      return false;
    }
  }
  init_perf_string(config_);
  LOG(INFO) << "SeqProcessor: " << *this << " initailized";
  return true;
}

Status SeqProcessor::peek(const int& idx, const std::vector<std::shared_ptr<const Frame>>& frames,
                          std::shared_ptr<Frame>& data) {
  Status ret = Status::SUCC;
  for (const auto& i : valid_) {
    ret = ops_[i]->peek(idx, frames, data);
    if (ignore_fail_ || ret == Status::SUCC || ret == Status::IGNORE) {
      continue;
    }
    LOG(ERROR) << "SeqProcessor: " << *this << " process failed";
    return ret;
  }
  return Status::SUCC;
}

Status SeqProcessor::process(const int& idx,
                             const std::vector<std::shared_ptr<const Frame>>& frames,
                             const std::vector<std::shared_ptr<const Frame>>& latests,
                             std::shared_ptr<Frame>& data) {
  Status ret = Status::SUCC;
  for (const auto& i : valid_) {
    ret = ops_[i]->process(idx, frames, latests, data);
    if (ret != Status::SUCC && !ignore_fail_) {
      LOG(ERROR) << *ops_[i] << " process failed";
      return ret;
    }
  }
  return ret;
}

}  // namespace framework
}  // namespace airi
}  // namespace crdc
