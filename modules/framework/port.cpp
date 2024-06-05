// Copyright (C) 2021 FengD
// License: Modified BSD Software License Agreement
// Author: Feng DING
// Description: Port is used to connect the data pipeline for each op.

#include "framework/port.h"

namespace crdc {
namespace airi {

DECLARE_int32(cached_data_expire_time);
DECLARE_int32(cached_data_tolerate_offset);

std::ostream& operator<<(std::ostream& os, const Port& port) {
  os << port.name();
  return os;
}

Port::Port() {}

bool Port::init(const size_t idx, const OperatorConfig& config, EventManager* event_manager,
                SharedDataManager* shared_data_manager,
                const std::shared_ptr<const EventMeta>& sub_event,
                const std::vector<EventMeta>& pub_events,
                const std::map<std::string, std::string>& event_data_map) {
  idx_ = idx;
  config_ = config;
  name_ = config_.name() + "[" + std::to_string(idx) + "]";

  CHECK(event_manager != NULL) << "event_manager == NULL";
  CHECK(shared_data_manager != NULL) << "shared_data_manager == NULL";
  event_manager_ = event_manager;
  shared_data_manager_ = shared_data_manager;

  is_input_ = true;
  if (sub_event) {
    sub_meta_event_.reset(new EventMeta(*sub_event));
    is_input_ = false;
  }
  pub_meta_events_ = pub_events;

  if (!init_trigger_data()) {
    LOG(ERROR) << "Failed to init trigger data for Port:" << name();
    return false;
  }
  if (!init_output_data()) {
    LOG(ERROR) << "Failed to init output data for Port:" << name();
    return false;
  }

  if (!init_input_data(event_data_map)) {
    LOG(ERROR) << "Failed to init input data for Port:" << name();
    return false;
  }

  if (!init_latest_data(event_data_map)) {
    LOG(ERROR) << "Failed to init latest shared data for Port:" << name();
    return false;
  }
  return true;
}

bool Port::init_input_data(const std::map<std::string, std::string>& event_data_map) {
  input_wait_.assign(config_.input_size(), -1);
  input_offset_.assign(config_.input_size(), 0ULL);
  input_window_.assign(config_.input_size(), FLAGS_cached_data_tolerate_offset);
  if (config_.input_offset_size() > 0 && config_.input_offset_size() != config_.input_size()) {
    LOG(ERROR) << "input_offset should be empty or same size with input: "
           << config_.input_offset_size() << " vs. " << config_.input_size();
    return false;
  }
  if (config_.input_window_size() > 0 && config_.input_window_size() != config_.input_size()) {
    LOG(ERROR) << "input_window should be empty or same size with input: "
           << config_.input_window_size() << " vs. " << config_.input_size();
    return false;
  }
  if (config_.input_wait_size() > 0 && config_.input_wait_size() != config_.input_size()) {
    LOG(ERROR) << "input_wait should be empty or same size with input: "
      << config_.input_wait_size() << " vs. " << config_.input_size();
    return false;
  }

  for (int i = 0; i < config_.input_offset_size(); ++i) {
    input_offset_[i] = static_cast<int>(config_.input_offset(i) * 1.e6);
  }
  for (int i = 0; i < config_.input_wait_size(); ++i) {
    if (config_.input_wait(i) > 0) {
      input_wait_[i] = static_cast<int>(config_.input_wait(i) * 1.e6);
    }
  }
  for (int i = 0; i < config_.input_window_size(); ++i) {
    input_window_[i] = config_.input_window(i);
    if (input_window_[i] < 0) {
      LOG(ERROR) << *this << " window max < 0: " << input_window_[i];
      return false;
    }
  }

  for (int i = 0; i < config_.input_size(); ++i) {
    LOG(INFO) << *this << " Input[" << i << "]"
          << " offset: " << input_offset_[i] << " ms"
          << " window: " << input_window_[i]
          << " wait(max): " << static_cast<double>(input_wait_[i] * 1.e-3) << " ms";
  }

  input_data_name_.assign(config_.input_size(), "");
  input_event_name_.assign(config_.input_size(), "");
  input_data_.assign(config_.input_size(), nullptr);
  for (int i = 0; i < config_.input_size(); ++i) {
    const std::string event_name = config_.input(i);
    input_event_name_[i] = event_name;
    if (is_input_) {
      continue;
    }
    if (event_data_map.find(event_name) == event_data_map.end()) {
      LOG(ERROR) << "Failed to find data from event:" << event_name;
      return false;
    }
    const std::string data_name = event_name + "_RO";
    if (std::find(input_data_name_.begin(), input_data_name_.end(), data_name) !=
        input_data_name_.end()) {
      LOG(WARNING) << "Duplicated input frame_data:" << data_name;
      return false;
    }
    LOG(INFO) << *this << " setup [input]: <" << event_name << "> [" << data_name << "]";
    FrameCachedData* data =
        dynamic_cast<FrameCachedData*>(shared_data_manager_->get_shared_data(data_name));
    if (data == nullptr) {
      LOG(ERROR) << "Failed to get [input] cached data:" << data_name
                 << ". " << *shared_data_manager_;
      return false;
    }
    input_data_[i] = data;
    input_data_name_[i] = data_name;
  }

  return true;
}

bool Port::init_latest_data(const std::map<std::string, std::string>& event_data_map) {
  // latest data config
  latest_data_.resize(config_.latest_size());
  latest_tolerate_offset_.assign(config_.latest_size(), -1);
  latest_data_name_.resize(config_.latest_size());
  latest_event_name_.resize(config_.latest_size());
  if (config_.latest_tolerate_offset_size() > 0 &&
              config_.latest_tolerate_offset_size() != config_.latest_size()) {
    LOG(ERROR) << *this << " latest_tolerate_offset size != lastest size (" <<
            config_.latest_tolerate_offset_size() << " vs. " << config_.latest_size() << ")";
    return false;
  }
  for (int i = 0; i < config_.latest_tolerate_offset_size(); ++i) {
    latest_tolerate_offset_[i] = static_cast<int>(config_.latest_tolerate_offset(i) * 1.e6);
  }
  for (int i = 0; i < config_.latest_size(); ++i) {
    latest_event_name_[i] = config_.latest(i);
  }

  // latest shared_data
  for (size_t i = 0; i < latest_event_name_.size(); ++i) {
    auto event_name = latest_event_name_[i];
    if (event_data_map.find(event_name) == event_data_map.end()) {
      LOG(ERROR) << "Failed to find data from event:" << event_name;
      return false;
    }
    // const std::string data_name = event_data_map.at(event_name) + "_RO";
    const std::string data_name = event_name + "_RO";
    latest_data_name_[i] = data_name;
    FrameCachedData* data =
        dynamic_cast<FrameCachedData*>(shared_data_manager_->get_shared_data(data_name));
    if (data == nullptr) {
      LOG(ERROR) << "Failed to get [latest] cached data:" << data_name
                 << ". " << *shared_data_manager_;
      return false;
    }
    latest_data_[i] = data;
  }
  return true;
}

bool Port::init_output_data() {
  if (config_.output_size() <= static_cast<int>(idx_)) {
    return true;
  }

  auto& output = config_.output(idx_);
  std::string trigger_name = config_.trigger_data(idx_);
  std::string output_name = output.data();

  if (output.has_reference()) {
    has_reference_ = true;
    ref_data_name_ = output.event() + "_RO";
    FrameCachedData* data =
        dynamic_cast<FrameCachedData*>(shared_data_manager_->get_shared_data(ref_data_name_));
    if (data == nullptr) {
      LOG(ERROR) << "Failed to get [output reference] cached data:" << ref_data_name_ << ". "
             << *shared_data_manager_;
      return false;
    }
    ref_data_ = data;
  }

  CHECK(trigger_data_);
  has_downstream_ = true;
  if (output.downstream_size() == 0) {
    if (output_name != trigger_name) {
      FrameCachedData* data =
          dynamic_cast<FrameCachedData*>(shared_data_manager_->get_shared_data(output_name));
      if (data == nullptr) {
        LOG(ERROR) << "Failed to get [output] cached data:" << output_name
               << ". " << *shared_data_manager_;
        return false;
      }

      unsigned int period = 0;
      unsigned int trigger_hz = trigger_data_->hz();
      if (data->hz() < trigger_hz) {
        period = 1000000 / data->hz() - 1000000 / 2 / trigger_hz;
      }

      output_data_name_.emplace_back(output_name);
      output_data_.emplace_back(data);
      output_event_name_ = "";
      output_last_.emplace_back(0);
      output_period_.emplace_back(period);

      output_copy_idx_.emplace_back(0);
      output_nocopy_idx_.clear();
    } else {
      has_downstream_ = false;
    }
    return true;
  }

  int trigger_hz = trigger_data_->hz();
  output_period_.resize(output.downstream_size(), 0);
  output_last_.assign(output.downstream_size(), 0);
  output_event_name_ = output.event();
  output_data_.assign(output.downstream_size(), nullptr);
  output_data_name_.assign(output.downstream_size(), "");
  for (int j = 0; j < output.downstream_size(); ++j) {
    auto& downstream = output.downstream(j);
    auto data_name = downstream.data();
    output_data_name_[j] = data_name;
    FrameCachedData* data =
        dynamic_cast<FrameCachedData*>(shared_data_manager_->get_shared_data(data_name));
    if (data == nullptr) {
      LOG(ERROR) << "Failed to get [output] cached data:"
                 << data_name << ". " << *shared_data_manager_;
      return false;
    }
    int data_hz = data->hz();
    if (data_hz < trigger_hz) {
      output_period_[j] = 1000000 / data_hz - 1000000 / 2 / trigger_hz;
    }
    output_data_[j] = data;
    if (!is_input_ && output_name == trigger_name && data_name == output_name) {
      LOG(INFO) << *this << " [output event]: " << output.event()
                << " Publish to DATA [" << data_name << "] (nocopy)";
      output_nocopy_idx_.emplace_back(j);
     continue;
    }
    LOG(INFO) << *this << " [output event]: " << output.event() << " Publish to DATA [" << data_name
          << "] (copy data)";
    output_copy_idx_.emplace_back(j);
  }

  return true;
}

bool Port::init_trigger_data() {
  const std::string event_name = config_.trigger(idx_);
  trigger_event_name_ = event_name;

  std::string data_name = config_.trigger_data(idx_);
  if (data_name.empty()) {
    if (static_cast<int>(idx_) >= config_.output_size()) {
      LOG(ERROR) << *this << " Failed to infer trigger data. trigger: "
             << "[" << event_name << "]";
      return false;
    }
    data_name = config_.output(idx_).data();
  }
  LOG(INFO) << *this << " trigger: <" << event_name << "> subscribe to DATA [" << data_name << "]";
  FrameCachedData* data =
      dynamic_cast<FrameCachedData*>(shared_data_manager_->get_shared_data(data_name));
  if (!data) {
    LOG(ERROR) << "Failed to get [trigger] cached data:" << data_name;
    return false;
  }
  trigger_data_ = data;
  trigger_data_name_ = data_name;

  return true;
}

bool Port::get_trigger_data(std::shared_ptr<Frame>* trigger, Event* sub_event) {
  if (!sub_meta_event_) {
    return false;
  }
  const EventMeta& event_meta = *sub_meta_event_;

  if (!event_manager_->subscribe(event_meta.event_id, sub_event)) {
    LOG(ERROR) << "Failed to subscribe. meta_event: <" << event_meta.to_string() << ">";
    return false;
  }
  if (sub_event->timestamp == 0) {
    LOG(ERROR) << "sub event timestamp is 0. meta_event: <" << event_meta.to_string() << ">";
    return false;
  }

  uint64_t timestamp = sub_event->timestamp;

  if (!trigger_data_->get(timestamp, trigger, 0)) {
    LOG(ERROR) << "Failed to get trigger data:" << trigger_data_name_;
    return false;
  }

  if (timestamp != (*trigger)->base_frame->utime) {
    LOG(ERROR) << "Failed to get trigger data:" << trigger_data_name_
               << "event timestamp: " << timestamp << ", data utime: "
               << (*trigger)->base_frame->utime;
    return false;
  }

  uint64_t now = get_now_microsecond();
  if (sub_event->local_timestamp > 0) {
    int dt = now - sub_event->local_timestamp;
    LOG(INFO) << *this << " FetchData: " << dt << " us";
  }

  LOG(INFO) << *this << " got data. dt: " << timestamp - last_ts_ << " us"
            << " event ts:" << timestamp << " s_last_ts:" << last_ts_;
  last_ts_ = timestamp;
  return true;
}

bool Port::get_input_data(const std::vector<uint64_t>& timestamps,
                          std::vector<std::shared_ptr<const Frame>>* frames) {
  if (timestamps.size() != input_data_.size()) {
    LOG(ERROR) << "input size is wrong. [" << timestamps.size() << ", "
               << input_data_.size() << "]";
    return false;
  }

  frames->resize(input_data_.size(), nullptr);
  for (size_t i = 0; i < input_data_.size(); ++i) {
    if (timestamps[i] != 0) {
      std::shared_ptr<Frame> input_data;
      if (!input_data_[i]->get(timestamps[i], &input_data, 0) ||
          timestamps[i] != input_data->base_frame->utime) {
        LOG(ERROR) << *this << " failed to get frame input: [" << input_data_name_[i] << "]"
                << " input timestamp:" << timestamps[i];
        return false;
      } else {
        frames->at(i) = input_data;
      }
    }
  }
  return true;
}

bool Port::get_input_data(uint64_t timestamp, std::vector<std::shared_ptr<const Frame>>* frames) {
  frames->resize(input_data_.size());
  for (size_t i = 0; i < input_data_.size(); ++i) {
    CHECK(input_data_[i]);
    uint64_t tstamp = timestamp + input_offset_[i];
    bool data_found = false;
    std::shared_ptr<Frame> input_data;
    if (!input_data_[i]->get(tstamp, &input_data, input_window_[i])) {
      LOG(WARNING) << *this << " failed to get frame input: <" << input_event_name_[i] << "> ["
               << input_data_name_[i] << "]"
               << " input timestamp:" << tstamp;
    } else {
      frames->at(i) = input_data;
      data_found = true;
    }
    // check every 5ms
    static const int check_dt = 2000;
    static const uint64_t max_expire_time = FLAGS_cached_data_expire_time * 1000000;
    if (!data_found && input_wait_[i] > 0) {
      const uint64_t expired_time = timestamp - max_expire_time;
      std::shared_ptr<const Frame>* input_ptr = &frames->at(i);
      if (input_data_[i]->get_newest(input_ptr) &&
                (*input_ptr)->base_frame->utime > expired_time) {
        std::shared_ptr<Frame> input_data;
        int trail = input_wait_[i] / check_dt + 1;
        for (int t = 0; t < trail; ++t) {
          LOG(WARNING) << *this << " input frame data:[" << input_event_name_[i] << "]"
                       << " try to fetch in " << check_dt << " us";
          std::this_thread::sleep_for(std::chrono::microseconds(static_cast<uint64_t>(check_dt)));
          if (input_data_[i]->get(tstamp, &input_data, input_window_[i])) {
            LOG(INFO) << *this << " input frame data:[" << input_event_name_[i] << "]"
                  << " input data found in " << (t + 1) * check_dt << " us";
            data_found = true;
            frames->at(i) = input_data;
            break;
          }
        }
      } else {
        LOG(WARNING) << *this << " input frame data:[" << input_event_name_[i]
                     << "] expired, skip";
      }
    }
    if (!data_found) {
      frames->at(i).reset();
      LOG(WARNING) << *this << " input frame data:[" << input_event_name_[i] << "]"
                   << " bundle failed";
    }
#ifdef DEBUG_BUNDLE
    if (frames->at(i)) {
      auto& bund_time = frames->at(i)->utime;
      int bundle_diff = (tstamp > bund_time) ? tstamp - bund_time : bund_time - tstamp;
      LOG(INFO) << "[BUNDLE TIME] bundle" << (*trigger)->sensor << " and " << frames->at(i)->sensor
            << ": " << bundle_diff;
    }
#endif
  }
  return true;
}

bool Port::get_latest_data(uint64_t timestamp, std::vector<std::shared_ptr<const Frame>>* frames) {
  frames->resize(latest_data_.size());
  for (size_t i = 0; i < latest_data_.size(); ++i) {
    CHECK(latest_data_[i]);
    if (!latest_data_[i]->get_newest(&frames->at(i))) {
      LOG(WARNING) << *this << " failed to get frame latest: <" << latest_event_name_[i] << "> ["
                   << latest_data_name_[i] << "]";
    } else if (latest_tolerate_offset_[i] > 0) {
      uint64_t dt = (timestamp > frames->at(i)->base_frame->utime ?
                    timestamp - frames->at(i)->base_frame->utime :
                    frames->at(i)->base_frame->utime - timestamp);
      if (dt > (uint64_t)latest_tolerate_offset_[i]) {
        frames->at(i) = nullptr;
      }
    }
  }
  return true;
}

bool Port::get_latest_data(const std::vector<uint64_t>& timestamps,
                            std::vector<std::shared_ptr<const Frame>>* frames) {
  if (timestamps.size() != latest_data_.size()) {
    LOG(ERROR) << "input size is wrong. [" << timestamps.size() << ", "
               << latest_data_.size() << "]";
    return false;
  }

  frames->resize(latest_data_.size(), nullptr);
  for (size_t i = 0; i < latest_data_.size(); ++i) {
    if (timestamps[i] != 0) {
      std::shared_ptr<Frame> latest_data;
      if (!latest_data_[i]->get(timestamps[i], &latest_data, 0) ||
          timestamps[i] != latest_data->base_frame->utime) {
        LOG(ERROR) << *this << " failed to get latest frame: [" << latest_data_name_[i] << "]"
                << " timestamp:" << timestamps[i];
        return false;
      } else {
        frames->at(i) = latest_data;
      }
    }
  }
  return true;
}

void Port::publish(const std::shared_ptr<Frame>& trigger) {
  const std::vector<EventMeta>& pub_meta_events = pub_meta_events_;
  const std::vector<int>& copy_idx = output_copy_idx_;
  const std::vector<int>& nocopy_idx = output_nocopy_idx_;

  auto ts = trigger->base_frame->utime;

  trigger->add_footprint(output_event_name_);
  uint64_t now = get_now_microsecond();
  if (ref_data_) {
    std::shared_ptr<Frame> data(new Frame(*trigger));
    if (!ref_data_->put(ts, data)) {
      LOG(ERROR) << *this << " Failed to PUT reference data: " << ref_data_name_;
    } else {
      LOG(INFO) << *this << " PUT reference data: " << ref_data_name_ << " utime:" << ts;
    }
  }
  if (!has_downstream_) {
    return;
  }
  for (auto& i : copy_idx) {
    if (ts > output_last_[i]
        && ((ts - output_last_[i]) < output_period_[i])) {
      LOG(ERROR) << *this << " skip to put data: " << output_data_name_[i];
      continue;
    }
    std::shared_ptr<Frame> data(new Frame(*trigger));
    if (!output_data_[i]->put(ts, data)) {
      LOG(ERROR) << *this << " Failed to put data: " << output_data_name_[i];
      continue;
    }
    if (output_event_name_.empty()) {
      LOG(INFO) << *this << " publish event(with data[" << output_data_name_[i]
          << "]): without event";
    } else {
      Event event;
      event.event_id = pub_meta_events[i].event_id;
      event.timestamp = ts;
      event.local_timestamp = now;
      this->event_manager_->publish(event);
      LOG(INFO) << *this << " publish event(with data[" << output_data_name_[i]
          << "]): " << pub_meta_events[i].name;
    }
    output_last_[i] = ts;
  }
  for (auto& i : nocopy_idx) {
    if (ts > output_last_[i]
       && ((ts - output_last_[i]) < output_period_[i])) {
      LOG(ERROR) << *this << " skip to put data: " << output_data_name_[i];
      continue;
    }
    Event event;
    event.event_id = pub_meta_events.at(i).event_id;
    event.timestamp = ts;
    event.local_timestamp = now;
    this->event_manager_->publish(event);
    LOG(INFO) << *this << " publish event ([" << output_data_name_[i]
          << "]): " << pub_meta_events[i].name;
    output_last_[i] = ts;
  }
}

}  // namespace airi
}  // namespace crdc
