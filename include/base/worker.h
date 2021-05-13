// Copyright (c) 2020 Tencent Corporation. All rights reserved.
#pragma once

#include <list>
#include <string>

#include "task.h"
#include "thread.h"

namespace tdf {
namespace base {
class WorkerPool;
class TaskRunner;
class Worker : public Thread {
 public:
  explicit Worker(const std::string& name = "");
  virtual ~Worker() = default;
  void Run();
  void Terminate();
  void Bind(std::shared_ptr<TaskRunner> runner);
  void Bind(std::list<std::shared_ptr<TaskRunner>> list);
  void UnBind(std::shared_ptr<TaskRunner> runner);
  std::list<std::shared_ptr<TaskRunner>> UnBind();
  std::list<std::shared_ptr<TaskRunner>> ReleasePending();
  std::list<std::shared_ptr<TaskRunner>> RetainFront();
  std::list<std::shared_ptr<TaskRunner>> Retain(std::shared_ptr<TaskRunner> runner);

  constexpr bool IsTerminated() const { return is_terminated_; }

 private:
  friend class WorkerPool;

  void Balance();
  void Sort();
  std::shared_ptr<Task> GetNextTask();

  std::shared_ptr<TaskRunner> curr_runner_;
  std::list<std::shared_ptr<TaskRunner>> running_runners_;
  std::list<std::shared_ptr<TaskRunner>> pending_runners_;
  std::mutex running_mutex_;
  std::mutex balance_mutex_;
  std::shared_ptr<std::condition_variable> cv_;
  bool need_balance_;
  bool is_terminated_;
};
}  // namespace base
}  // namespace tdf
