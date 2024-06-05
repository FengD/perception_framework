// Copyright (C) 2021 FengD
// License: Modified BSD Software License Agreement
// Author: Feng DING
// Description: Frame is used to transform data in framework
//              It contents a base frame. If need, we could add extra data in it
//              Directly or create soled struct and put it in the base frame

#pragma once

#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <boost/any.hpp>

namespace crdc {
namespace airi {

class CustomData;

/** 
 * @struct BaseFrame
 * @brief This Frame is the base frame which used in Frame
 */
struct BaseFrame {
  BaseFrame(): utime(0LL), recv_utime(0LL), sender("") {}

  BaseFrame(const BaseFrame& frame):
    utime(frame.utime),
    recv_utime(frame.recv_utime),
    sender(frame.sender),
    data(frame.data) {}

  BaseFrame& operator =(const BaseFrame& frame) {
    utime = frame.utime;
    recv_utime = frame.recv_utime;
    sender = frame.sender;
    data = frame.data;
    return *this;
  }

  // time in microsecond
  uint64_t utime = 0LL;
  // received time in microsecond
  uint64_t recv_utime = 0LL;
  std::string sender;
  std::shared_ptr<CustomData> data;
};

/**
 * @brief The Frame used for transport
 */
struct Frame {
  Frame();
  Frame(const Frame& frame);
  virtual ~Frame() = default;

  template <typename T>
  explicit Frame(const T& msg) : Frame() {
    this->deserialize<T>(msg);
  }

  template <typename T>
  Frame(const std::string& channel, const T& msg) : Frame() {
    this->deserialize<T>(channel, msg);
  }
  void reset() { }

  template <typename T>
  bool serialize(T* msg) const;

  /**
   * @brief deserialize from message type
   * @param[in] msg message
   * @note  implement when necessary. *ONLY* used for objects modification, do *not*
   *        touch other fields.
   */
  template <typename T>
  void deserialize(const T& msg);

  /**
   * @brief deserialize from subscribed LCM message type
   * @param[in] channel LCM channel name
   * @param[in] msg LCM message
   * @note  implement for each subscribed LCM message. ALL fields may be cleaned.
   */
  template <typename T>
  void deserialize(const std::string& channel, const T& msg);

  template <typename T>
  void deserialize(const std::string& channel, const std::shared_ptr<T>& msg);

  /**
   * @brief check if the frame has certained footprint
   * @param[in] the footprint [std::string]
   * @return has footprint [bool]
   */
  bool has_footprint(const std::string& fp) const;

  /**
   * @brief Add footprint in the frame. The footprint is added by event in port. And
   *        named by event name.
   * @param[in] the footprint [std::string]
   */
  void add_footprint(const std::string& fp) const;

  /**
   * @brief print the footprints
   * @return the string list of the footprints[std::string]
   */
  std::string footprints() const;
  std::string frame_type;
  std::shared_ptr<BaseFrame> base_frame = nullptr;
  mutable std::unordered_map<std::string, boost::any> supplement;

 private:
  Frame& operator =(const Frame& frame);
  mutable std::mutex fp_lock_;
  // the footprint history list
  mutable std::set<std::string> footprint_;
};

}  // namespace airi
}  // namespace crdc
