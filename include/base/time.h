//
// Copyright (c) 2020 The tencent Authors. All rights reserved.
//
#pragma once

#include <chrono>
namespace tdf {
using Clock = std::chrono::steady_clock;
using TimePoint = std::chrono::time_point<Clock>;
using Duration = std::chrono::nanoseconds;
constexpr Duration kDefaultFrameBudget = Duration(std::chrono::nanoseconds(1000000000LL / 60LL));
}  // namespace tdf
