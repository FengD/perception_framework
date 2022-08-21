// Copyright (C) 2021 Hirain Technologies
// License: Modified BSD Software License Agreement
// Author: Feng DING
// Description: Frame is used to transform data in framework
//              It contents a base frame. If need, we could add extra data in it
//              Directly or create soled struct and put it in the base frame

#include "framework/frame.h"
#include <algorithm>
#include <sstream>
#include <iterator>

namespace crdc {
namespace airi {

Frame::Frame()
    : frame_type(""),
      base_frame(new BaseFrame) {}

Frame::Frame(const Frame& frame) {
  {
    std::unique_lock<std::mutex> lock(frame.fp_lock_);
    footprint_ = frame.footprint_;
  }
  base_frame.reset(new BaseFrame(*frame.base_frame));
  frame_type = frame.frame_type;
  supplement = frame.supplement;
}

bool Frame::has_footprint(const std::string& fp) const {
  std::unique_lock<std::mutex> lock(fp_lock_, std::try_to_lock);
  if (!lock) {
    return false;
  }
  bool has = footprint_.count(fp);
  return has;
}

void Frame::add_footprint(const std::string& fp) const {
  std::unique_lock<std::mutex> lock(fp_lock_);
  footprint_.emplace(fp);
}

std::string Frame::footprints() const {
  std::unique_lock<std::mutex> lock(fp_lock_);
  std::ostringstream fp_ss;
  std::copy(footprint_.begin(), footprint_.end(), std::ostream_iterator<std::string>(fp_ss, ","));
  return fp_ss.str();
}
}  // namespace airi
}  // namespace crdc
