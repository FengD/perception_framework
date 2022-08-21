#pragma once

#include <memory>
#include <string>
#include <vector>

#include "framework/operator.h"
namespace crdc {
namespace airi {

class TestOperator : public Operator {
 public:
  TestOperator();
  virtual ~TestOperator() = default;
  void run() override;
  void stop() override {
    stop_ = true;
    Operator::stop();
  }
};

REGISTER_OPERATOR(TestOperator);
}  // namespace airi
}  // namespace crdc
