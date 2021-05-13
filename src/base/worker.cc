// Copyright (c) 2020 Tencent Corporation. All rights reserved.

#include "base/worker.h"

#include <algorithm>
#include <atomic>

#include "base/logging.h"
#include "base/worker_pool.h"

namespace tdf {
namespace base {

Worker::Worker(const std::string& name) : Thread(name), is_terminated_(false) {
  cv_ = std::make_shared<std::condition_variable>();
}

void Worker::Balance() {
  // running_mutex_ has locked before balance
  std::lock_guard<std::mutex> lock(balance_mutex_);

  Duration time;  // default 0
  if (!running_runners_.empty()) {
    // Sort is executed before balance
    time = running_runners_.front()->GetTime();
  }
  for (auto it = pending_runners_.begin(); it != pending_runners_.end(); ++it) {
    (*it)->SetTime(time);
  }
  running_runners_.splice(running_runners_.end(), pending_runners_);
  // The first taskrunner still has the highest priority
}

void Worker::Run() {
  while (!is_terminated_) {
    auto task = GetNextTask();
    if (task && !task->IsCanceled()) {
      TimePoint begin = Clock::now();
      task->Run();
      curr_runner_->AddTime(Clock::now() - begin);
    }
  }
}

void Worker::Sort() {
  running_runners_.sort([](const auto lhs, const auto rhs) {
    return lhs->GetPriority() * lhs->GetTime() < rhs->GetPriority() * rhs->GetTime();
  });
}

void Worker::Terminate() {
  is_terminated_ = true;
  cv_->notify_one();
  Join();
}

void Worker::Bind(std::shared_ptr<TaskRunner> runner) {
  std::lock_guard<std::mutex> lock(balance_mutex_);

  pending_runners_.insert(pending_runners_.end(), runner);
  need_balance_ = true;
  cv_->notify_one();
}

void Worker::Bind(std::list<std::shared_ptr<TaskRunner>> list) {
  std::lock_guard<std::mutex> lock(balance_mutex_);

  pending_runners_.splice(pending_runners_.end(), list);
  need_balance_ = true;
  cv_->notify_one();
}

void Worker::UnBind(std::shared_ptr<TaskRunner> runner) {
  std::lock_guard<std::mutex> lock(running_mutex_);

  auto it = std::find_if(running_runners_.begin(), pending_runners_.end(),
                         [runner](const auto each) { return each->GetId() == runner->GetId(); });
  if (it != running_runners_.end()) {
    running_runners_.erase(it);
  }
}

std::list<std::shared_ptr<TaskRunner>> Worker::UnBind() {
  std::lock_guard<std::mutex> lock(running_mutex_);

  std::list<std::shared_ptr<TaskRunner>> ret(std::move(running_runners_));
  running_runners_ = std::list<std::shared_ptr<TaskRunner>>{};
  return ret;
}

std::list<std::shared_ptr<TaskRunner>> Worker::ReleasePending() {
  std::lock_guard<std::mutex> lock(balance_mutex_);

  std::list<std::shared_ptr<TaskRunner>> ret(std::move(pending_runners_));
  pending_runners_ = std::list<std::shared_ptr<TaskRunner>>{};
  return ret;
}

std::list<std::shared_ptr<TaskRunner>> Worker::RetainFront() {
  std::lock_guard<std::mutex> lock(running_mutex_);

  if (running_runners_.empty()) {
    return std::list<std::shared_ptr<TaskRunner>>();
  }

  Sort();
  auto it = running_runners_.begin();
  std::shared_ptr<TaskRunner> runner(*it);
  running_runners_.erase(it);
  std::list<std::shared_ptr<TaskRunner>> ret(std::move(running_runners_));
  running_runners_ = std::list<std::shared_ptr<TaskRunner>>{runner};
  return ret;
}

std::list<std::shared_ptr<TaskRunner>> Worker::Retain(std::shared_ptr<TaskRunner> runner) {
  std::lock_guard<std::mutex> lock(running_mutex_);

  auto it = std::find_if(running_runners_.begin(), pending_runners_.end(),
                         [runner](const auto each) { return each->GetId() == runner->GetId(); });
  if (it != running_runners_.end()) {
    running_runners_.erase(it);
  }
  std::list<std::shared_ptr<TaskRunner>> ret(std::move(running_runners_));
  running_runners_ = std::list<std::shared_ptr<TaskRunner>>{runner};
  return ret;
}

std::shared_ptr<Task> Worker::GetNextTask() {
  if (is_terminated_) {
    return nullptr;
  }

  std::unique_lock<std::mutex> lock(running_mutex_);

  Sort();
  if (need_balance_) {
    Balance();
    need_balance_ = false;
  }

  Duration min_time, time;
  TimePoint now;
  for (auto it = running_runners_.begin(); it != running_runners_.end(); ++it) {
    auto task = (*it)->GetNext();
    if (task) {
      curr_runner_ = *it;
      return task;
    } else {
      if (now == TimePoint()) {  // uninitialized
        now = Clock::now();
        min_time = Duration::max();
        time = min_time;
      }
      time = (*it)->GetNextDuration(now);
      if (min_time > time) {
        min_time = time;
      }
    }
  }

  if (min_time != Duration::max() && min_time != Duration::zero()) {
    cv_->wait_for(lock, min_time);
  } else {
    cv_->wait(lock);
  }
  // to do
  return nullptr;
}
}  // namespace base
}  // namespace tdf
