#pragma once

#include <functional>
#include <memory>

#include "task_runner.h"
#include "time_delta.h"
#include "time_point.h"

namespace footstone {
namespace base {

class BaseTimer {
 public:
  BaseTimer() = default;
  explicit BaseTimer(std::shared_ptr<TaskRunner> task_runner);
  virtual ~BaseTimer();

  void Stop();
  void Reset();
  inline void BindTaskRunner(std::shared_ptr<TaskRunner> task_runner) {
    task_runner_ = task_runner;
  }
  inline bool IsRunning() { return is_running_; }

 protected:
  virtual void RunUserTask() = 0;
  virtual void OnStop() = 0;
  void ScheduleNewTask(TimeDelta delay);
  void StartInternal(TimeDelta delay);

  std::weak_ptr<TaskRunner> task_runner_;
  std::unique_ptr<Task> user_task_;
  TimeDelta delay_;

 private:
  void OnScheduledTaskInvoked();

  bool is_running_;
  TimePoint desired_run_time_;
  TimePoint scheduled_run_time_;

  TDF_BASE_DISALLOW_COPY_AND_ASSIGN(BaseTimer);
};

}  // namespace base
}  // namespace footstone
