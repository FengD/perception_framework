// Copyright (C) 2021 Hirain Technologies
// License: Modified BSD Software License Agreement
// Author: Feng DING
// Description: gflags

#include <gflags/gflags.h>

namespace crdc {
namespace airi {

/// used in shared data
DEFINE_int32(shared_data_stale_time, 2,
             "the time threshold longer than which the data becomes stale, in second");
/// used in cached data
DEFINE_int32(cached_data_stale_time, 2,
             "the time threshold longer than which the data becomes stale, in second");
DEFINE_int32(cached_data_tolerate_offset, 5, "tolerate search range");
DEFINE_int32(cached_data_expire_time, 60,
             "the time threshold longer than which do not need to bundle, in second");

/// used in dag_streaming
DEFINE_int32(max_allowed_congestion_value, 0,
             "When DAGStreaming event_queues max length greater than "
             "max_allowed_congestion_value, reset DAGStreaming."
             "(default is 0, disable this feature.)");
DEFINE_bool(enable_timing_remove_stale_data, true, "whether timing clean shared data");

/// used in event_manager
DEFINE_int32(max_event_queue_size, 1, "The max size of event queue.");

/// used in framework_main
DEFINE_string(dag_config_path, "./conf/dag_streaming.config", "Onboard DAG Streaming config.");

}  // namespace airi
}  // namespace crdc

