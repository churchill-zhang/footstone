// Copyright (c) 2020 Tencent Corporation. All rights reserved.

#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>

#include "macros.h"
#include "task.h"
#include "time.h"

namespace tdf {
namespace base {
class TaskRunner {
 public:
  std::string label_;

  explicit TaskRunner(std::string label, bool is_excl = false, int64_t priority = 1);
  ~TaskRunner();

  void Clear();
  void Terminate();

  virtual std::shared_ptr<Task> PostTask(std::shared_ptr<Task> task) {
    std::lock_guard<std::mutex> lock(mutex_);

    task_queue_.push(task);
    if (cv_) {
      cv_->notify_one();
    }

    return task;
  }
  template <typename F, typename... Args>
  std::shared_ptr<Task> PostTask(F&& f, Args... args) {
    using T = typename std::result_of<F(Args...)>::type;
    auto task = std::make_shared<std::packaged_task<T()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));

    return PostTask(std::make_shared<Task>([task]() { (*task)(); }));
  }

  template <typename T>
  std::shared_ptr<FutureTask<T>> PostFutureTask(std::shared_ptr<FutureTask<T>> task) {
    std::lock_guard<std::mutex> lock(mutex_);

    task_queue_.push(task);
    if (cv_) {
      cv_->notify_one();
    }

    return task;
  }

  template <typename F, typename... Args>
  auto PostFutureTask(F&& f, Args... args) -> std::shared_ptr<FutureTask<F, Args...>> {
    auto task =
        std::make_shared<FutureTask<F, Args...>>(std::forward<F>(f), std::forward<Args>(args)...);
    PostFutureTask(task);
    return task;
  }

  void PostDelayedTask(std::shared_ptr<Task> task, Duration delay);

  template <typename F, typename... Args>
  auto PostDelayedTask(F&& f, Duration delay, Args... args) {
    using T = typename std::result_of<F(Args...)>::type;
    auto task = std::make_shared<std::packaged_task<T()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));

    auto delay_task = std::make_shared<Task>([task]() { (*task)(); });
    PostDelayedTask(delay_task, delay);

    return delay_task;
  }

  Duration GetNextDuration(TimePoint now);

  inline bool GetExclusive() { return is_excl_; }
  inline int64_t GetPriority() { return priority_; }
  inline int64_t GetId() { return id_; }
  inline Duration GetTime() { return time_; }
  inline Duration AddTime(Duration time) {
    time_ = time_ + time;
    return time_;
  }

  inline void SetTime(Duration time) { time_ = time; }

  std::shared_ptr<Task> GetNext();
  void Run();

 private:
  friend class Worker;
  friend class Scheduler;
  friend class WorkerPool;

  // TaskRunner(bool is_excl = false);

  std::shared_ptr<Task> popTaskFromDelayedQueueNoLock(TimePoint now);

  std::shared_ptr<Task> PopTask();
  std::shared_ptr<Task> GetTopDelayTask();

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
  bool is_terminated_;
  bool is_excl_;
  int64_t priority_;
  int64_t id_;
  Duration time_;

  TDF_BASE_DISALLOW_COPY_AND_ASSIGN(TaskRunner);
};

}  // namespace base
}  // namespace tdf
