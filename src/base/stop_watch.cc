// Copyright (c) 2020 The tencent Authors. All rights reserved.
//
// Created by willisdai on 2020/11/13.
//
#include "base/stop_watch.h"

#include <utility>

namespace tdf {
namespace base {
std::ostream &operator<<(std::ostream &out, StopWatch &sw) {
  out << "StopWatch[" << sw.tag_ << "] " << sw.begin_.time_since_epoch().count() << " --> "
      << sw.end_.time_since_epoch().count() << ", " << sw.delta_.count();
  return out;
}

StopWatch::StopWatch(bool auto_start) {
  if (auto_start) {
    Start();
  }
}

void StopWatch::Start() {
  if (started_) {
    return;
  }
  begin_ = Clock::now();
  started_ = true;
}

void StopWatch::Stop(std::string tag) {
  if (!started_) {
    return;
  }
  end_ = Clock::now();
  delta_ = end_ - begin_;
  started_ = false;
  tag_ = std::move(tag);
}

ScopedStopWatch::ScopedStopWatch(StopWatchCallback callback, std::string tag, bool is_auto_start)
    : stop_watch_(is_auto_start), callback_(std::move(callback)), tag_(std::move(tag)) {}

ScopedStopWatch::~ScopedStopWatch() {
  stop_watch_.Stop(tag_);
  callback_(stop_watch_);
}

StopWatch &ScopedStopWatch::GetWatch() { return stop_watch_; }
}  // namespace base
}  // namespace tdf
