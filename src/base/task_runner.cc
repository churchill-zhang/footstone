// Copyright (c) 2020 Tencent Corporation. All rights reserved.

#include "base/task_runner.h"

#include <atomic>

#include "base/logging.h"
#include "base/trace_event.h"

namespace tdf {
namespace base {
std::atomic<int64_t> global_task_runner_id{0};

TaskRunner::TaskRunner(std::string label, bool is_excl, int64_t priority)
    : label_(std::move(label)),
      cv_(nullptr),
      is_terminated_(false),
      is_excl_(is_excl),
      priority_(priority),
      time_(Duration::zero()) {
  id_ = global_task_runner_id.fetch_add(1);
}
TaskRunner::~TaskRunner() {}

void TaskRunner::Clear() {
  std::unique_lock<std::mutex> lock(mutex_);

  while (!task_queue_.empty()) {
    task_queue_.pop();
  }

  while (!delayed_task_queue_.empty()) {
    delayed_task_queue_.pop();
  }
}

void TaskRunner::Terminate() {
  is_terminated_ = true;

  Clear();
}

void TaskRunner::PostDelayedTask(std::shared_ptr<Task> task, Duration delay) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (is_terminated_) {
    return;
  }

  TimePoint deadline = Clock::now() + delay;
  delayed_task_queue_.push(std::make_pair(deadline, task));

  cv_->notify_one();
}

std::shared_ptr<Task> TaskRunner::PopTask() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!task_queue_.empty()) {
    std::shared_ptr<Task> result = std::move(task_queue_.front());
    task_queue_.pop();
    return result;
  }

  return nullptr;
}

std::shared_ptr<Task> TaskRunner::GetTopDelayTask() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (task_queue_.empty() && !delayed_task_queue_.empty()) {
    std::shared_ptr<Task> result =
        std::move(const_cast<DelayedEntry&>(delayed_task_queue_.top()).second);
    return result;
  }
  return nullptr;
}

std::shared_ptr<Task> TaskRunner::GetNext() {
  std::unique_lock<std::mutex> lock(mutex_);

  if (is_terminated_) {
    return nullptr;
  }

  if (!task_queue_.empty()) {
    std::shared_ptr<Task> result = std::move(task_queue_.front());
    task_queue_.pop();
    return result;
  }

  TimePoint now = Clock::now();
  return popTaskFromDelayedQueueNoLock(now);
}

void TaskRunner::Run() {
  while (auto task = GetNext()) {
    if (task && !task->IsCanceled()) {
      task->Run();
    }
  }
}

void TaskRunner::SetCv(std::shared_ptr<std::condition_variable> cv) { cv_ = cv; }

Duration TaskRunner::GetNextDuration(TimePoint now) {
  std::unique_lock<std::mutex> lock(mutex_);
  if (task_queue_.empty() && !delayed_task_queue_.empty()) {
    const DelayedEntry& delayed_task = delayed_task_queue_.top();
    return delayed_task.first - now;
  } else {
    return Duration::max();
  }
}

std::shared_ptr<Task> TaskRunner::popTaskFromDelayedQueueNoLock(TimePoint now) {
  if (delayed_task_queue_.empty()) {
    return nullptr;
  }

  const DelayedEntry& deadline_and_task = delayed_task_queue_.top();
  if (deadline_and_task.first > now) {
    return nullptr;
  }

  std::shared_ptr<Task> result = std::move(const_cast<DelayedEntry&>(deadline_and_task).second);
  delayed_task_queue_.pop();
  return result;
}

}  // namespace base
}  // namespace tdf
