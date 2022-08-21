#pragma once

#include <memory>
#include <string>
#include <vector>
#include "framework/framework.h"

namespace crdc {
namespace airi {

class TestOp : public Op {
 public:
  TestOp();
  virtual ~TestOp() = default;

  bool init(const std::string &config_path) override;

  Status process(int idx, const std::vector<std::shared_ptr<const Frame>> &frames,
                 std::shared_ptr<Frame> &data) override;

  std::string name() const override { return "TestOp"; }
};

class TestOp2 : public TestOp {
 public:
  TestOp2() = default;
  virtual ~TestOp2() = default;
  std::string name() const override { return "TestOp2"; }
};

class TestOp3 : public TestOp {
 public:
  TestOp3() = default;
  virtual ~TestOp3() = default;
  std::string name() const override { return "TestOp3"; }
};

REGISTER_OP(TestOp);
REGISTER_OP(TestOp2);
REGISTER_OP(TestOp3);

}  // namespace airi
}  // namespace crdc
