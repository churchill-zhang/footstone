// Copyright (c) 2020 Tencent Corporation. All rights reserved.
//
// Created by willisdai on 2020/10/30.
//
#pragma once
#include <chrono>
#include <functional>
#include <iostream>
#include <string>
#include "base/time.h"

namespace tdf {
namespace base {
class StopWatch {
 public:
  explicit StopWatch(bool auto_start = true);

  void Start();

  void Stop(std::string tag = "");

  const TimePoint &GetBegin() const { return begin_; }

  const TimePoint &GetEnd() const { return end_; }

  const Duration &GetDeltaMs() const { return delta_; }

  friend std::ostream &operator<<(std::ostream &out, StopWatch &sw);

 private:
  bool started_ = false;
  TimePoint begin_;
  TimePoint end_;
  Duration delta_;
  std::string tag_ = "";
};

class ScopedStopWatch {
 public:
  using StopWatchCallback = std::function<void(const StopWatch &sw)>;
  explicit ScopedStopWatch(StopWatchCallback callback,
                           std::string tag = "",
                           bool is_auto_start = true);
  virtual ~ScopedStopWatch();

  StopWatch &GetWatch();

 protected:
  StopWatch stop_watch_;

 private:
  StopWatchCallback callback_;
  std::string tag_ = "";
};
}  // namespace base
}  // namespace tdf
