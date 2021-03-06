#include "footstone/base_timer.h"

namespace footstone {
inline namespace timer {

BaseTimer::BaseTimer(std::shared_ptr<TaskRunner> task_runner)
    : user_task_(nullptr), task_runner_(task_runner) {}

BaseTimer::~BaseTimer() {}

void BaseTimer::Stop() {
  is_running_ = false;
  OnStop();
}

void BaseTimer::ScheduleNewTask(TimeDelta delay) {
  std::shared_ptr<TaskRunner> task_runner = task_runner_.lock();
  if (!task_runner) {
    return;
  }

  is_running_ = true;
  std::weak_ptr<BaseTimer> weak_self = shared_from_this();
  if (delay > TimeDelta::Zero()) {
    task_runner->PostDelayedTask(std::make_unique<Task>([weak_self]{
      auto self = weak_self.lock();
      if (!self) {
        return;
      }
      self->OnScheduledTaskInvoked();
    }), delay);
    scheduled_run_time_ = desired_run_time_ = TimePoint::Now() + delay;
  } else {
    task_runner->PostTask(std::make_unique<Task>([weak_self]{
      auto self = weak_self.lock();
      if (!self) {
        return;
      }
      self->OnScheduledTaskInvoked();
    }));
    scheduled_run_time_ = desired_run_time_ = TimePoint::Now();
  }
}

void BaseTimer::OnScheduledTaskInvoked() {
  if (!is_running_) {
    return;
  }

  if (desired_run_time_ > scheduled_run_time_) {
    TimePoint now = TimePoint::Now();
    if (desired_run_time_ > now) {
      ScheduleNewTask(desired_run_time_ - now);
      return;
    }
  }

  RunUserTask();
}

void BaseTimer::StartInternal(TimeDelta delay) {
  delay_ = delay;

  Reset();
}

void BaseTimer::Reset() {
  if (scheduled_run_time_ < TimePoint::Now()) {
    ScheduleNewTask(delay_);
    return;
  }

  if (delay_ > TimeDelta::Zero()) {
    desired_run_time_ = TimePoint::Now() + delay_;
  } else {
    desired_run_time_ = TimePoint::Now();
  }

  if (desired_run_time_ >= scheduled_run_time_) {
    is_running_ = true;
    return;
  }

  ScheduleNewTask(delay_);
}

}  // namespace timer
}  // namespace footstone
