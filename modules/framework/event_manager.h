// Copyright (C) 2021 FengD
// License: Modified BSD Software License Agreement
// Author: Feng DING
// Description: Event Manager

#pragma once

#include <gflags/gflags.h>

#include <memory>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <algorithm>

#include "common/common.h"
#include "framework/event.h"
#include "framework/proto/dag_config.pb.h"

namespace crdc {
namespace airi {

DECLARE_int32(max_event_queue_size);

class EventManager {
 public:
  EventManager() = default;
  ~EventManager() = default;

  // not thread-safe.
  bool init(const std::vector<EventMeta>& events, const DAGConfig& dag_config);

  // thread-safe.
  bool publish(const Event& event);

  // if no event arrive, this api would be block.
  // thread-safe.
  bool subscribe(EventID event_id, Event* event);

  bool subscribe(EventID event_id, Event* event, bool nonblocking);

  // clear all the event queues.
  void reset();
  int avg_len_of_event_queues() const;
  int max_len_of_event_queues() const;

  bool get_event_meta(EventID event_id, EventMeta* event_meta) const;
  bool get_event_meta(const std::vector<EventID>& event_ids,
                      std::vector<EventMeta>* event_metas) const;

  int num_events() const { return event_queue_map_.size(); }

 protected:
  std::vector<std::vector<int>> traverse(const int& idx, const std::vector<std::vector<int>>& adj);

 private:
  friend class crdc::airi::common::Singleton<EventManager>;

  using EventQueue = crdc::airi::common::FixedSizeConQueue<Event>;
  using EventQueueMap = std::unordered_map<EventID, std::unique_ptr<EventQueue>>;
  using EventQueueMapIterator = EventQueueMap::iterator;
  using EventQueueMapConstIterator = EventQueueMap::const_iterator;
  using EventMetaMap = std::unordered_map<EventID, EventMeta>;
  using EventMetaMapIterator = EventMetaMap::iterator;
  using EventMetaMapConstIterator = EventMetaMap::const_iterator;

  bool get_event_queue(EventID event_id, EventQueue** queue);

  EventQueueMap event_queue_map_;
  // for debug.
  EventMetaMap event_meta_map_;
  bool inited_ = false;

  std::vector<std::vector<EventID>> pipelines_;

  DISALLOW_COPY_AND_ASSIGN(EventManager);
};

}  // namespace airi
}  // namespace crdc
