// Copyright 2020 Tencent
#pragma once
#include "scheduler.h"
#include "task_runner.h"
#include "thread.h"
#include "worker.h"

namespace tdf {
namespace base {
class WorkerPool : Scheduler {
 public:
  static std::shared_ptr<WorkerPool> GetInstance(int64_t size);
  explicit WorkerPool(int64_t size);
  virtual ~WorkerPool();
  void Resize(int64_t size);
  std::shared_ptr<TaskRunner> CreateTaskRunner(std::string label, bool is_excl = false,
                                               int64_t priority = 1);

 private:
  friend class Profile;

  void CreateWorker(int64_t size, bool is_excl = false);

  void IncreaseThreads(int64_t new_size);
  void ReduceThreads(int64_t new_size);

  static std::mutex creation_mutex_;
  static std::shared_ptr<WorkerPool> instance_;

  std::vector<std::unique_ptr<Worker>> excl_workers_;
  std::vector<std::unique_ptr<Worker>> workers_;
  std::vector<std::shared_ptr<TaskRunner>> runners_;

  int64_t index_;
  int64_t size_;
  std::mutex mutex_;

  TDF_BASE_DISALLOW_COPY_AND_ASSIGN(WorkerPool);
};

}  // namespace base
}  // namespace tdf
