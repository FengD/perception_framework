// Copyright (C) 2021 Hirain Technologies
// License: Modified BSD Software License Agreement
// Author: Feng DING
// Description: Event Manager

#include "framework/event_manager.h"

namespace crdc {
namespace airi {

bool EventManager::init(const std::vector<EventMeta>& events, const DAGConfig& dag_config) {
  if (inited_) {
    LOG(WARNING) << "EventManager init twice.";
    return true;
  }

  for (auto& event_meta : events) {
    if (event_queue_map_.find(event_meta.event_id) != event_queue_map_.end()) {
      LOG(ERROR) << "duplicate event id in config. id: " << event_meta.event_id;
      return false;
    }
    event_queue_map_[event_meta.event_id].reset(new EventQueue(FLAGS_max_event_queue_size));
    event_meta_map_.emplace(event_meta.event_id, event_meta);
    LOG(INFO) << "Load EventMeta: " << event_meta.to_string();
  }

  LOG(INFO) << "Load " << event_queue_map_.size() << " events in DAGSreaming.";

  std::vector<EventID> event_ids(events.size());
  std::unordered_map<EventID, int> event_id_map;
  for (size_t i = 0; i < events.size(); ++i) {
    event_ids[i] = events[i].event_id;
    event_id_map[events[i].event_id] = i;
  }
  std::vector<std::vector<int>> adj(events.size(), std::vector<int>(events.size(), 0));
  for (size_t i = 0; i < events.size(); ++i) {
    auto& e1 = events[i];
    int e1_idx = event_id_map[e1.event_id];
    for (size_t j = 0; j < events.size(); ++j) {
      if (i == j) {
        continue;
      }
      auto& e2 = events[j];
      int e2_idx = event_id_map[e2.event_id];
      if (e1.to_node == e2.from_node) {
        adj[e1_idx][e2_idx] = 1;
      }
    }
  }

  std::vector<int> heads;
  for (size_t i = 0; i < events.size(); ++i) {
    int sum = 0;
    for (size_t j = 0; j < events.size(); ++j) {
      sum += adj[j][i];
    }
    if (sum == 0) {
      heads.emplace_back(i);
    }
  }
  for (auto& h : heads) {
    auto& event = event_meta_map_[event_ids[h]];
    LOG(INFO) << "Event Head: " << event.name;
    for (auto& p : traverse(h, adj)) {
      std::vector<EventID> link;
      std::transform(p.rbegin(), p.rend(), std::back_inserter(link),
                     [&](const int i) -> EventID { return event_ids[i]; });
      pipelines_.emplace_back(link);
    }
  }

  LOG(INFO) << "Event Pipelines: " << pipelines_.size();
  for (size_t i = 0; i < pipelines_.size(); ++i) {
    std::stringstream ss;
    ss << "Event Pipeline #" << i << std::endl;
    for (auto& e : pipelines_[i]) {
      if (event_meta_map_.find(e) == event_meta_map_.end()) {
        LOG(ERROR) << "No such event: " << e;
        continue;
      }
      auto& event = event_meta_map_.at(e);
      ss << "    * " << event.name << std::endl;
    }
    LOG(INFO) << ss.str();
  }

  inited_ = true;
  return true;
}

std::vector<std::vector<int>>
  EventManager::traverse(const int& idx, const std::vector<std::vector<int>>& adj) {
  std::vector<std::vector<int>> ret;
  int sum = 0;
  for (auto& c : adj.at(idx)) {
    sum += c;
  }
  if (sum == 0) {
    ret.emplace_back(std::vector<int>{idx});
    return ret;
  }
  for (size_t i = 0; i < adj.at(idx).size(); ++i) {
    if (adj.at(idx).at(i)) {
      for (auto s : traverse(i, adj)) {
        s.emplace_back(idx);
        ret.emplace_back(s);
      }
    }
  }
  return ret;
}

bool EventManager::publish(const Event& event) {
  EventQueue* queue = NULL;
  if (!get_event_queue(event.event_id, &queue)) {
    return false;
  }

  if (!queue->try_push(event)) {
    // Critical errors: queue is full.
    LOG(ERROR) << "EventQueue is FULL. id: " << event.event_id
           << ", name: " << event_meta_map_[event.event_id].name;
    // Clear all blocked data.
    LOG(ERROR) << "clear EventQueue. id: " << event.event_id << " size: " << queue->size();
    queue->clear();

    // try second time.
    queue->try_push(event);
  }

  return true;
}

bool EventManager::subscribe(EventID event_id, Event* event, bool nonblocking) {
  EventQueue* queue = NULL;
  if (!get_event_queue(event_id, &queue)) {
    return false;
  }

  if (nonblocking) {
    return queue->try_pop(event);
  }

  LOG(INFO) << "EVENT_ID: " << event_id
    << ", NAME: " << event_meta_map_[event_id].name
    << ", QUEUE LENGTH:" << queue->size();
  queue->pop(event);
  return true;
}

bool EventManager::subscribe(EventID event_id, Event* event) {
  return subscribe(event_id, event, false);
}

bool EventManager::get_event_queue(EventID event_id, EventQueue** queue) {
  EventQueueMapIterator iter = event_queue_map_.find(event_id);
  if (iter == event_queue_map_.end()) {
    LOG(ERROR) << "event: " << event_id << " not exist in EventQueueMap.";
    return false;
  }

  CHECK(queue != NULL) << " event_id: " << event_id;
  *queue = iter->second.get();
  return true;
}

bool EventManager::get_event_meta(EventID event_id, EventMeta* event_meta) const {
  EventMetaMapConstIterator citer = event_meta_map_.find(event_id);
  if (citer == event_meta_map_.end()) {
    LOG(WARNING) << "event not found in EventManager. id: " << event_id;
    return false;
  }

  *event_meta = citer->second;
  return true;
}

bool EventManager::get_event_meta(const std::vector<EventID>& event_ids,
                                  std::vector<EventMeta>* event_metas) const {
  event_metas->reserve(event_ids.size());
  for (EventID event_id : event_ids) {
    EventMeta meta;
    if (!get_event_meta(event_id, &meta)) {
      return false;
    }
    event_metas->emplace_back(meta);
  }
  return true;
}

int EventManager::avg_len_of_event_queues() const {
  if (event_queue_map_.empty()) {
    return 0;
  }

  int total_length = 0;
  for (const auto& event : event_queue_map_) {
    total_length += event.second->size();
  }
  return total_length / event_queue_map_.size();
}

int EventManager::max_len_of_event_queues() const {
  int max_length = 0;
  for (const auto& event : event_queue_map_) {
    max_length = std::max(max_length, event.second->size());
  }
  return max_length;
}

void EventManager::reset() {
  EventQueueMapIterator iter = event_queue_map_.begin();
  for (; iter != event_queue_map_.end(); ++iter) {
    iter->second->clear();
  }
}

}  // namespace airi
}  // namespace crdc
