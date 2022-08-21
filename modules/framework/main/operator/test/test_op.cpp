#include "operator/test/test_op.h"

namespace crdc {
namespace airi {

TestOp::TestOp() : Op() {}

bool TestOp::init(const std::string &config_path) {
  LOG(INFO) << this->name() << " init";
  return true;
}

Status TestOp::process(int idx, const std::vector<std::shared_ptr<const Frame>> &frames,
                       std::shared_ptr<Frame> &data) {
  data->frame_type = data->frame_type + "," + this->name();
  LOG(WARNING) << this->name() << " " << idx
    << " process " << frames.size() << " " << data->frame_type;
  return Status::SUCC;
}

}  // namespace airi
}  // namespace crdc
