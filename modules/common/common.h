// Copyright (C) 2021 Hirain Technologies
// License: Modified BSD Software License Agreement
// Author: Feng DING
// Description: common

#pragma once

#include <iostream>
#include "cyber/cyber.h"
#include "common/concurrent_object_pool.h"
#include "common/concurrent_queue.h"
#include "common/error_code.h"
#include "common/factory.h"
#include "common/for_each.h"
#include "common/macros.h"
#include "common/singleton.h"
#include "common/thread.h"
#include "common/thread_safe_queue.h"
#include "common/io/file.h"

#define MAX_THREAD_NAME_LENGTH 21

static uint64_t get_now_microsecond() {
  auto now = std::chrono::high_resolution_clock::now();
  auto now_us = std::chrono::time_point_cast<std::chrono::microseconds>(now);
  return now_us.time_since_epoch().count();
}
