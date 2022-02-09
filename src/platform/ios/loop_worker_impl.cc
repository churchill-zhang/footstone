#include "footstone/platform/ios/loop_worker_impl.h"

#include <cmath>

namespace footstone {
inline namespace runner {

#ifdef IOS_WORKER_TIME_INTERVAL
static constexpr CFTimeInterval kInterval = IOS_WORKER_TIME_INTERVAL;
#else
static constexpr CFTimeInterval kInterval = 1.0e10;
#endif

LoopWorkerImpl::MainWorker() : loop_(CFRunLoopGetMain()) {
  CFRunLoopTimerContext context = {
      .info = this,
  };
  delayed_wake_timer_ = CFRunLoopTimerCreate(kCFAllocatorDefault, kInterval, HUGE_VAL, 0, 0,
                                             reinterpret_cast<CFRunLoopTimerCallBack>(&MainWorker::OnTimerFire),
                                             &context);
}

LoopWorkerImpl::~MainWorker() {
  CFRunLoopTimerInvalidate(delayed_wake_timer_);
  CFRunLoopRemoveTimer(loop_, delayed_wake_timer_, kCFRunLoopDefaultMode);
}

std::unique_ptr<Task> LoopWorkerImpl::DoGetNextTask() {
  return GetNextTask();
}

void LoopWorkerImpl::OnRun() {
  while (!is_terminated_) {
    int result = CFRunLoopRunInMode(kCFRunLoopDefaultMode, kInterval, true);
    if (result == kCFRunLoopRunStopped || result == kCFRunLoopRunFinished) {
      RunTask();
      is_terminated_ = true;
    }
  }
}

void LoopWorkerImpl::Notify() {
  CFRunLoopTimerSetNextFireDate(
      delayed_wake_timer_,
      CFAbsoluteTimeGetCurrent());
}

void LoopWorkerImpl::OnTimerFire(CFRunLoopTimerRef timer, MainWorker *worker) {
  worker->RunTask();
}

void LoopWorkerImpl::WaitFor(const TimeDelta &delta) {
  CFRunLoopTimerSetNextFireDate(
      delayed_wake_timer_,
      CFAbsoluteTimeGetCurrent() + delta.ToSecondsF());
}

}
}