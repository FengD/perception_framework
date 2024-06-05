// Copyright (C) 2021 FengD
// License: Modified BSD Software License Agreement
// Author: Feng DING
// Description: for each

#pragma once

#include <type_traits>
#include "common/common.h"

namespace crdc {
namespace airi {
namespace common {
DEFINE_TYPE_TRAIT(HasLess, operator<)  // NOLINT

template <class Value, class End>
typename std::enable_if<HasLess<Value>::value && HasLess<End>::value,
                        bool>::type
LessThan(const Value& val, const End& end) {
  return val < end;
}

template <class Value, class End>
typename std::enable_if<!HasLess<Value>::value || !HasLess<End>::value,
                        bool>::type
LessThan(const Value& val, const End& end) {
  return val != end;
}

#define FOR_EACH(i, begin, end)           \
  for (auto i = (true ? (begin) : (end)); \
    crdc::airi::common::LessThan(i, (end)); ++i)
}  // namespace common
}  // namespace airi
}  // namespace crdc
