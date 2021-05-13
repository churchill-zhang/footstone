// Copyright (c) 2020 Tencent Corporation. All rights reserved.

#include "base/trace_event.h"
#include <chrono>
#include "base/timeline.h"

namespace tdf {
namespace base {

#if TDF_TIMELINE_ENABLED

using std::chrono::duration_cast;
using std::chrono::microseconds;
using std::chrono::steady_clock;

#define NOW_US duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count()

void TraceEvent(std::string name) { TraceTimelineEvent(name, NOW_US, Timeline_Event_Begin); }

void TraceEventEnd(std::string name) { TraceTimelineEvent(name, NOW_US, Timeline_Event_End); }

void TraceEventInstant(std::string name) {
  TraceTimelineEvent(name, NOW_US, Timeline_Event_Instant);
}

void TraceTimelineEvent(std::string name, int64_t timestamp_micros, Timeline_Event_Type type) {
  if (type < Timeline_Event_Begin) {
    return;
  }
  if (type > Timeline_Event_Instant) {
    return;
  }
  auto event = Timeline::Recorder()->ThreadBlockStartEvent();
  if (event == nullptr) {
    return;
  }
  switch (type) {
    case Timeline_Event_Begin:
      event->Begin(name, timestamp_micros);
      break;
    case Timeline_Event_End:
      event->End(name, timestamp_micros);
      break;
    case Timeline_Event_Instant:
      event->Instant(name, timestamp_micros);
      break;
  }
  event->Complete();
}

#else  // DF_TIMELINE_ENABLED

void TraceEvent(TraceArg name) {}

void TraceEventEnd(TraceArg name) {}

void TraceEventInstant(TraceArg name) {}

void TraceTimelineEvent(TraceArg name, int64_t timestamp_micros, Tdf_Timeline_Event_Type type) {}

#endif

ScopedTraceEventStopWatch::ScopedTraceEventStopWatch(std::string event, StopWatchCallback callback)
  : stop_watch_(false), callback_(std::move(callback)), event_(std::move(event)) {
  TraceEvent(event_);
  stop_watch_.Start();
}

ScopedTraceEventStopWatch::~ScopedTraceEventStopWatch() {
  stop_watch_.Stop();
  TraceEventEnd(event_);
  callback_(stop_watch_);
}

}  // namespace base
}  // namespace tdf
