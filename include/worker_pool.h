#pragma once

#include <mutex>
#include "task_runner.h"
#include "thread.h"
#include "worker.h"

namespace footstone {
inline namespace runner {

class WorkerPool {
 public:
  static std::shared_ptr<WorkerPool> GetInstance(int size);
  explicit WorkerPool(int size);
  ~WorkerPool();
  void Terminate();
  void Resize(int size);
  std::shared_ptr<TaskRunner> CreateTaskRunner(bool is_excl = false, int priority = 1,
                                               const std::string& name = "");
  void RemoveTaskRunner(std::shared_ptr<TaskRunner> runner);

 private:
  friend class Profile;
  void CreateWorker(int size, bool is_excl = false);
  void BindWorker(std::shared_ptr<Worker> worker,
                              std::vector<std::shared_ptr<TaskRunner>> group);
  void AddTaskRunner(std::shared_ptr<TaskRunner> runner);

  static std::mutex creation_mutex_;
  static std::shared_ptr<WorkerPool> instance_;

  std::vector<std::shared_ptr<Worker>> excl_workers_;
  std::vector<std::shared_ptr<Worker>> workers_;
  std::vector<std::shared_ptr<TaskRunner>> runners_;

  int index_;
  int size_;
  std::mutex mutex_;

  TDF_BASE_DISALLOW_COPY_AND_ASSIGN(WorkerPool);
};

}  // namespace runner
}  // namespace footstone
