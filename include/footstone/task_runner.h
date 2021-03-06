#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>
#include <memory>
#include <cstdint>

#include "macros.h"
#include "task.h"
#include "idle_task.h"
#include "worker.h"
#include "time_delta.h"
#include "time_point.h"

namespace footstone {
inline namespace runner {

constexpr uint32_t kDefaultGroupId = 0;

class TaskRunner {
 public:
  using TimePoint = time::TimePoint;
  using TimeDelta = time::TimeDelta;
  // group_id 0 代表不分组，业务可以通过指定group_id（非0）强制不同TaskRunner在同一个Worker中运行
  TaskRunner(uint32_t group_id = kDefaultGroupId, uint32_t priority = 1,
             bool is_schedulable = true, std::string name = "");
  ~TaskRunner();

  void Clear();
  bool AddSubTaskRunner(const std::shared_ptr<TaskRunner> &sub_runner,
                        bool is_task_running = false);
  bool RemoveSubTaskRunner(const std::shared_ptr<TaskRunner> &sub_runner);
  void PostTask(std::unique_ptr<Task> task);
  template<typename F, typename... Args>
  void PostTask(F &&f, Args... args) {
    using T = typename std::result_of<F(Args...)>::type;
    auto packaged_task = std::make_shared<std::packaged_task<T()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    auto task = std::make_unique<Task>([packaged_task]() { (*packaged_task)(); });
    PostTask(std::move(task));
  }

  void PostDelayedTask(std::unique_ptr<Task> task, TimeDelta delay);

  template<typename F, typename... Args>
  void PostDelayedTask(F &&f, TimeDelta delay, Args... args) {
    using T = typename std::result_of<F(Args...)>::type;
    auto packaged_task = std::make_shared<std::packaged_task<T()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    auto task = std::make_unique<Task>([packaged_task]() { (*packaged_task)(); });
    PostDelayedTask(std::move(task), delay);
  }
  TimeDelta GetNextTimeDelta(TimePoint now);

  int32_t RunnerKeyCreate(const std::function<void(void *)> &destruct);
  bool RunnerKeyDelete(int32_t key);
  bool RunnerSetSpecific(int32_t key, void *p);
  void *RunnerGetSpecific(int32_t key);
  void RunnerDestroySpecifics();

  inline int32_t GetPriority() { return priority_; }
  inline int32_t GetId() { return id_; }
  inline std::string GetName() {
    return name_;
  }
  inline uint32_t GetGroupId() {
    return group_id_;
  }
  inline TimeDelta GetTime() { return time_; }
  inline TimeDelta AddTime(TimeDelta time) {
    time_ = time_ + time;
    return time_;
  }
  inline void SetTime(TimeDelta time) { time_ = time; }
  inline bool IsSchedulable() { return is_schedulable_; }

  // 必须要在 task 运行时调用 GetCurrentTaskRunner 才能得到正确的 Runner，task 运行之外调用将会abort
  static std::shared_ptr<TaskRunner> GetCurrentTaskRunner();

  void PostIdleTask(std::unique_ptr<IdleTask> task);
 private:
  friend class Worker;
  friend class Scheduler;
  friend class WorkerManager;
  friend class IdleTimer;

  void NotifyWorker();
  std::unique_ptr<Task> popTaskFromDelayedQueueNoLock(TimePoint now);
  std::unique_ptr<Task> PopTask();
  // 友元IdleTimer调用
  std::unique_ptr<IdleTask> PopIdleTask();
  std::unique_ptr<Task> GetTopDelayTask();
  std::unique_ptr<Task> GetNext();

  std::queue<std::unique_ptr<Task>> task_queue_;
  std::mutex queue_mutex_;
  std::queue<std::unique_ptr<IdleTask>> idle_task_queue_;
  std::mutex idle_mutex_;
  using DelayedEntry = std::pair<TimePoint, std::unique_ptr<Task>>;
  struct DelayedEntryCompare {
    bool operator()(const DelayedEntry &left, const DelayedEntry &right) const {
      return left.first > right.first;
    }
  };
  std::priority_queue<DelayedEntry, std::vector<DelayedEntry>, DelayedEntryCompare>
      delayed_task_queue_;
  std::mutex delay_mutex_;
  std::weak_ptr<Worker> worker_;
  std::string name_;
  bool has_sub_runner_;
  uint32_t priority_;
  uint32_t id_;
  uint32_t group_id_; // 业务可以通过指定group_id强制不同TaskRunner在同一个Worker中运行
  TimeDelta time_;
  /*
   *  is_schedulable_ 是否可调度
   *  很多第三方库使用了thread_local变量，调度器无法在迁移taskRunner的同时迁移第三方库的Thread_local变量,
   *  诸如此类的TaskRunner就应该设置为不可调度。但是TaskRunner的是否调度与Worker的是否调度是两个概念，虽然
   *  不可调度的TaskRunner不会被迁移，但其所在的Worker还是可以加入其他TaskRunner
   */
  bool is_schedulable_;
  TDF_BASE_DISALLOW_COPY_AND_ASSIGN(TaskRunner);
};

}  // namespace runner
}  // namespace footstone
