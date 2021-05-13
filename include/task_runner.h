// Copyright (c) 2020 Tencent Corporation. All rights reserved.

#ifndef TDF_BASE_INCLUDE_TASK_RUNNER_H_
#define TDF_BASE_INCLUDE_TASK_RUNNER_H_

#include <condition_variable>
#include <mutex>
#include <queue>

#include "macros.h"
#include "task.h"
#include "time_delta.h"
#include "time_point.h"

namespace base {

class TaskRunner {
 public:
  TaskRunner(bool is_excl = false, int priority = 1);
  ~TaskRunner();

  void Clear();
  void Terminate();

  std::shared_ptr<Task> PostTask(std::shared_ptr<Task> task) {
    std::lock_guard<std::mutex> lock(mutex_);

    task_queue_.push(task);
    cv_->notify_one();
    return task;
  }

  inline std::shared_ptr<TaskRunner> GetRefTaskRunner() { return ref_; }

  inline void SetRefTaskRunner(std::shared_ptr<TaskRunner> ref) { ref_ = ref; }

  template <typename F, typename... Args>
  std::shared_ptr<Task> PostTask(F&& f, Args... args) {
    using T = typename std::result_of<F(Args...)>::type;
    auto task = std::make_shared<std::packaged_task<T()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));

    return PostTask(std::make_shared<Task>([task]() { (*task)(); }));
  }

  template <typename T>
  std::shared_ptr<FutureTask<T>> PostFutureTask(std::shared_ptr<FutureTask<T>> task) {
    PostTask(task);
    return task;
  }

  template <typename F, typename... Args>
  auto PostFutureTask(F&& f, Args... args)
      -> std::shared_ptr<FutureTask<typename std::result_of<F(Args...)>::type>> {
    using T = typename std::result_of<F(Args...)>::type;
    auto task = std::make_shared<FutureTask<std::function<T(Args...)>>>(
        std::forward<F>(f), std::forward<Args>(args)...);
    PostFutureTask(task);
    return task;
  }

  void PostDelayedTask(std::shared_ptr<Task> task, TimeDelta delay);

  template <typename F, typename... Args>
  auto PostDelayedTask(F&& f, TimeDelta delay, Args... args) {
    using T = typename std::result_of<F(Args...)>::type;
    auto task = std::make_shared<std::packaged_task<T()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));

    auto delay_task = std::make_shared<Task>([task]() { (*task)(); });
    PostDelayedTask(delay_task, delay);

    return delay_task;
  }

  TimeDelta GetNextTimeDelta(TimePoint now);

  inline bool GetExclusive() { return is_excl_; }
  inline int GetPriority() { return priority_; }
  inline int GetId() { return id_; }
  inline TimeDelta GetTime() { return time_; }
  inline TimeDelta AddTime(TimeDelta time) {
    time_ = time_ + time;
    return time_;
  }

  inline void SetTime(TimeDelta time) { time_ = time; }

 private:
  friend class Worker;
  friend class Scheduler;
  friend class WorkerPool;

  // TaskRunner(bool is_excl = false);

  std::shared_ptr<Task> popTaskFromDelayedQueueNoLock(TimePoint now);

  std::shared_ptr<Task> PopTask();
  std::shared_ptr<Task> GetTopDelayTask();
  std::shared_ptr<Task> GetNext();

  void SetCv(std::shared_ptr<std::condition_variable> cv);

  std::queue<std::shared_ptr<Task>> task_queue_;
  using DelayedEntry = std::pair<TimePoint, std::shared_ptr<Task>>;
  struct DelayedEntryCompare {
    bool operator()(const DelayedEntry& left, const DelayedEntry& right) const {
      return left.first > right.first;
    }
  };
  std::priority_queue<DelayedEntry, std::vector<DelayedEntry>, DelayedEntryCompare>
      delayed_task_queue_;
  std::mutex mutex_;
  std::shared_ptr<std::condition_variable> cv_;
  std::shared_ptr<TaskRunner> ref_;
  bool is_terminated_;
  bool is_excl_;
  int priority_;
  int id_;
  TimeDelta time_;

  TDF_BASE_DISALLOW_COPY_AND_ASSIGN(TaskRunner);
};
}  // namespace base

#endif  // TDF_BASE_INCLUDE_TASK_RUNNER_H_
