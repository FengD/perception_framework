// Copyright (C) 2021 FengD
// License: Modified BSD Software License Agreement
// Author: Feng DING
// Description: Op is the minimum unit of execution. One operator could have
//              a single or multipule Ops. The op could be resumed or pause

#include "framework/op.h"

namespace crdc {
namespace airi {

std::ostream& operator<<(std::ostream& os, const Op& op) {
  os << "Op[" << op.name() << "]<" << OpType_Name(op.type()) << ">";
  return os;
}

void Op::set_trigger(const std::vector<std::string>& data,
                     const std::vector<std::string>& event) {
  trigger_data_name_ = data;
  trigger_event_name_ = event;
}

void Op::set_event_io(const std::vector<std::string>& input,
                      const std::vector<std::string>& output,
                      const std::vector<std::string>& latest) {
  input_event_name_ = input;
  output_event_name_ = output;
  latest_event_name_ = latest;
}
}  // namespace airi
}  // namespace crdc
