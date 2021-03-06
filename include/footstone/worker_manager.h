#pragma once

#include <mutex>
#include "task_runner.h"
#include "thread.h"
#include "worker.h"

namespace footstone {
inline namespace runner {

class WorkerManager {
 public:
  static std::shared_ptr<WorkerManager> GetDefaultManager();

  explicit WorkerManager(int size);
  ~WorkerManager();
  void Terminate();
  void Resize(int size);
  void AddWorker(std::unique_ptr<Worker> worker);
  std::shared_ptr<TaskRunner> CreateTaskRunner(uint32_t group_id = 0, uint32_t priority = 1,
                                               bool is_schedulable = true, const std::string &name = "");
  void RemoveTaskRunner(const std::shared_ptr<TaskRunner>& runner);

 private:
  friend class Profile;
  static void MoveTaskRunnerSpecificNoLock(int32_t runner_id,
                                           const std::shared_ptr<Worker>& from,
                                           const std::shared_ptr<Worker>& to);

  void CreateWorkers(int size);
  static void UpdateWorkerSpecific(const std::shared_ptr<Worker>& worker,
                            const std::vector<std::shared_ptr<TaskRunner>>& group);
  void Balance(int32_t increase_worker_count);
  void AddTaskRunner(std::shared_ptr<TaskRunner> runner);


  static std::mutex creation_mutex_;
  static std::shared_ptr<WorkerManager> instance_;

  std::vector<std::shared_ptr<Worker>> workers_;
  std::vector<std::shared_ptr<TaskRunner>> runners_;

  int index_;
  int size_;
  std::mutex mutex_;

  TDF_BASE_DISALLOW_COPY_AND_ASSIGN(WorkerManager);
};

}  // namespace runner
}  // namespace footstone
