#include "operator/test/test_operator.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace crdc {
namespace airi {

TestOperator::TestOperator() : Operator() {}

void TestOperator::run() {
  std::thread a([&](){
    while (1) {
      std::this_thread::sleep_for(std::chrono::microseconds(1000000));
      std::shared_ptr<Frame> frame(new Frame);
      frame->frame_type = this->name();
      frame->base_frame->utime = get_now_microsecond();
      process_and_publish(0, frame, false);
    }
  });
  a.detach();
}

}  // namespace airi
}  // namespace crdc
