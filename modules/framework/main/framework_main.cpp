#include <gflags/gflags.h>
#include <string>
#include "framework/framework.h"

#define MODULE "FRAMEWORK_MAIN"
DEFINE_string(config_file, "params/framework/production/dag_streaming.prototxt",
              "path of config file");

namespace crdc {
namespace airi {

std::vector<std::function<void(void)>> s_stop_callback;

static void stop(int signum = -1) {
  static bool in_stop = false;
  if (in_stop) {
    return;
  }
  in_stop = true;
  for (auto& f : s_stop_callback) {
    f();
  }
}

int main(int argc, char* argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  apollo::cyber::GlobalData::Instance()->SetProcessGroup(MODULE);
  apollo::cyber::Init(MODULE);

  if (!std::getenv("CRDC_WS")) {
    LOG(FATAL) << "[" << MODULE << "] CRDC_WS not setting!";
  } else {
    LOG(INFO) << "[" << MODULE << "] Current CRDC_WS: "
              << std::string(std::getenv("CRDC_WS"));
  }

  std::shared_ptr<DAGStreaming> dag_streaming(new DAGStreaming);
  std::string dag_config_path = "";
  dag_config_path = std::string(std::getenv("CRDC_WS")) + '/' + FLAGS_config_file;
  LOG(INFO) << "[" << MODULE << "] Use proto config: " << dag_config_path;
  if (!dag_streaming->init(dag_config_path)) {
    LOG(ERROR) << "[" << MODULE << "] Failed to Init DAGStreaming. dag_config_path:"
               << dag_config_path;
    stop();
    return 1;
  }

  s_stop_callback.emplace_back([&dag_streaming]() { dag_streaming->stop(); });
  dag_streaming->start();
  apollo::cyber::WaitForShutdown();
  LOG(WARNING) << "[" << MODULE << "] camera_driver terminated.";
  stop();
  dag_streaming->join();
  return 0;
}
}  // namespace airi
}  // namespace crdc

int main(int argc, char* argv[]) { return crdc::airi::main(argc, argv); }

