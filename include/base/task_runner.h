#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>

#include "macros.h"
#include "task.h"
#include "worker.h"
#include "time.h"
#include "time_delta.h"
#include "time_point.h"

namespace footstone {
namespace base {
class TaskRunner {
 public:
  explicit TaskRunner(bool is_excl = false, int priority = 1, const std::string& name = "");
  ~TaskRunner();

  void Clear();
  void Terminate();
  void AddSubTaskRunner(std::shared_ptr<TaskRunner> sub_runner, bool is_task_running = false);
  void RemoveSubTaskRunner(std::shared_ptr<TaskRunner> sub_runner);
  void PostTask(std::unique_ptr<Task> task);
  template <typename F, typename... Args>
  void PostTask(F&& f, Args... args) {
    using T = typename std::result_of<F(Args...)>::type;
    auto packaged_task = std::make_shared<std::packaged_task<T()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    auto task = std::make_unique<Task>([packaged_task]() { (*packaged_task)(); });
    PostTask(std::move(task));
  }

  void PostDelayedTask(std::unique_ptr<Task> task, TimeDelta delay);

  template <typename F, typename... Args>
  void PostDelayedTask(F&& f, Duration delay, Args... args) {
    using T = typename std::result_of<F(Args...)>::type;
    auto packaged_task = std::make_shared<std::packaged_task<T()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    auto task = std::make_unique<Task>([packaged_task]() { (*packaged_task)(); });
    TimeDelta delay_time = TimeDelta::FromNanoseconds(delay.count());
    PostDelayedTask(std::move(task), delay_time);
  }
  TimeDelta GetNextTimeDelta(TimePoint now);

  int32_t RunnerKeyCreate(std::function<void(void*)> destruct);
  bool RunnerKeyDelete(int32_t key);
  bool RunnerSetSpecific(int32_t key, void* p);
  void* RunnerGetSpecific(int32_t key);
  void RunnerDestroySpecifics();

  inline bool GetExclusive() { return is_excl_; }
  inline int32_t GetPriority() { return priority_; }
  inline int32_t GetId() { return id_; }
  inline std::string GetName() {
    return name_;
  }
  inline TimeDelta GetTime() { return time_; }
  inline TimeDelta AddTime(TimeDelta time) {
    time_ = time_ + time;
    return time_;
  }

  inline void SetTime(TimeDelta time) { time_ = time; }

  // 必须要在 task 运行时调用 GetCurrentTaskRunner 才能得到正确的 Runner，task 运行之外调用都将返回 nullptr
  static std::shared_ptr<TaskRunner> GetCurrentTaskRunner();

 private:
  friend class Worker;
  friend class Scheduler;
  friend class WorkerPool;

  std::unique_ptr<Task> popTaskFromDelayedQueueNoLock(TimePoint now);

  std::unique_ptr<Task> PopTask();
  std::unique_ptr<Task> GetTopDelayTask();
  std::unique_ptr<Task> GetNext();

  void SetCv(std::shared_ptr<std::condition_variable> cv);

  std::queue<std::unique_ptr<Task>> task_queue_;
  using DelayedEntry = std::pair<TimePoint, std::unique_ptr<Task>>;
  struct DelayedEntryCompare {
    bool operator()(const DelayedEntry& left, const DelayedEntry& right) const {
      return left.first > right.first;
    }
  };
  std::priority_queue<DelayedEntry, std::vector<DelayedEntry>, DelayedEntryCompare>
      delayed_task_queue_;
  std::mutex mutex_;
  std::shared_ptr<std::condition_variable> cv_;
  std::weak_ptr<Worker> worker_;
  bool is_terminated_;
  bool is_excl_;
  std::string name_;
  bool has_sub_runner_;
  int32_t priority_;
  int32_t id_;
  TimeDelta time_;

  TDF_BASE_DISALLOW_COPY_AND_ASSIGN(TaskRunner);
};

}  // namespace base
}  // namespace footstone
