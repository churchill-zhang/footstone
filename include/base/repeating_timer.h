#pragma once

#include "base_timer.h"

namespace footstone {
inline namespace timer {

class RepeatingTimer : public BaseTimer {
 public:
  using TaskRunner = runner::TaskRunner;

  RepeatingTimer() = default;
  explicit RepeatingTimer(std::shared_ptr<TaskRunner> task_runner);
  virtual ~RepeatingTimer();

  virtual void Start(std::unique_ptr<Task> user_task, TimeDelta delay);

 private:
  void OnStop() final;
  void RunUserTask() override;

  TDF_BASE_DISALLOW_COPY_AND_ASSIGN(RepeatingTimer);
};

}  // namespace base
}  // namespace footstone
