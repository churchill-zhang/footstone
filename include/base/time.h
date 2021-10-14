//
//
// Copyright (c) Tencent Corporation. All rights reserved.
//
//
#pragma once

#include <chrono>
namespace footstone {
using Clock = std::chrono::steady_clock;
using TimePoint = std::chrono::time_point<Clock>;
using Duration = std::chrono::nanoseconds;
constexpr Duration kDefaultFrameInterval = Duration(std::chrono::nanoseconds(1000000000LL / 60LL));
}  // namespace footstone
