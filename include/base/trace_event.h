// Copyright (c) 2020 Tencent Corporation. All rights reserved.

#pragma once
#include <stdio.h>
#include <string>
#include "stop_watch.h"

#if defined(RELEASE)
#define TDF_TIMELINE_ENABLED 0
#else
#define TDF_TIMELINE_ENABLED 1
#endif

#define __FML__TOKEN_CAT__(x, y) x##y
#define __FML__AUTO_TRACE_END(name) \
  ::tdf::base::ScopedInstantEnd __FML__TOKEN_CAT__(__trace_end_, __LINE__)(name)

#define TRACE_EVENT(name)        \
  ::tdf::base::TraceEvent(name); \
  __FML__AUTO_TRACE_END(name)

#define TRACE_EVENT_INSTANT(name) ::tdf::base::TraceEventInstant(name)

typedef enum {
  Timeline_Event_Begin,    // Phase = 'B'.
  Timeline_Event_End,      // Phase = 'E'.
  Timeline_Event_Instant,  // Phase = 'i'.
} Timeline_Event_Type;

namespace tdf {
namespace base {

void TraceEvent(std::string name);

void TraceEventEnd(std::string name);

void TraceEventInstant(std::string name);

void TraceTimelineEvent(std::string name, int64_t timestamp_micros, Timeline_Event_Type type);

class ScopedInstantEnd {
 public:
  explicit ScopedInstantEnd(std::string str) : label_(str) {}

  ~ScopedInstantEnd() { TraceEventEnd(label_); }

 private:
  std::string label_;

  ScopedInstantEnd(const ScopedInstantEnd &) = delete;
  ScopedInstantEnd &operator=(const ScopedInstantEnd &) = delete;
};

class ScopedTraceEventStopWatch {
 public:
  using StopWatchCallback = std::function<void(const StopWatch &sw)>;
  ScopedTraceEventStopWatch(std::string event, StopWatchCallback callback);
  ~ScopedTraceEventStopWatch();

 private:
  StopWatch stop_watch_;
  StopWatchCallback callback_;
  std::string event_ = "";
};
}  // namespace base
}  // namespace tdf
