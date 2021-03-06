#include "footstone/worker.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <map>
#include <utility>

#include "footstone/logging.h"
#include "footstone/worker_manager.h"

#ifdef ANDROID
#include "footstone/platform/adr/loop_worker_impl.h"
#elif defined IOS
#include "footstone/platform/ios/loop_worker_impl.h"
#endif

namespace footstone {
inline namespace runner {

std::atomic<int32_t> global_worker_id{0};
thread_local int32_t local_worker_id;
thread_local bool is_task_running = false;
thread_local std::shared_ptr<TaskRunner> local_runner;
thread_local std::vector<std::shared_ptr<TaskRunner>> curr_group;

Worker::Worker(bool is_schedulable, std::string name)
    : is_schedulable_(is_schedulable), name_(std::move(name)), is_terminated_(false),
      need_balance_(false), is_stacking_mode_(false), min_wait_time_(TimeDelta::Max()),
      next_task_time_(TimePoint::Max()), has_migration_data_(false), is_exit_immediately_(true) {}

Worker::~Worker() {
  TDF_BASE_CHECK(is_terminated_) << "Terminate function must be called before destruction";
  Worker::WorkerDestroySpecifics();
}

int32_t Worker::GetCurrentWorkerId() { return local_worker_id; }
std::shared_ptr<TaskRunner> Worker::GetCurrentTaskRunner() {
  TDF_BASE_CHECK(is_task_running) << "GetCurrentTaskRunner cannot be run outside of the task ";

  return local_runner;
}

bool Worker::HasUnschedulableRunner() {
  std::lock_guard<std::mutex> lock(running_mutex_);
  for (const auto &group: running_groups_) {
    for (const auto &runner: group) {
      if (!runner->IsSchedulable()) {
        return true;
      }
    }
  }
  return false;
}

void Worker::BalanceNoLock() {
  if (pending_groups_.empty()) {
    return;
  }

  TimeDelta time;  // default 0
  auto running_it = running_groups_.begin();
  if (running_it != running_groups_.end()) {
    time = running_it->front()->GetTime();
  }
  // 等待队列初始化为当前优先级最高taskRunner运行时间
  for (auto &group : pending_groups_) {
    for (auto &runner : group) {
      runner->SetTime(time);
    }
  }
  // 新的taskRunner插入到最高优先级之后，既可以保证原有队列执行顺序不变，又可以使得未执行的TaskRunner优先级高
  running_groups_.splice(running_it, pending_groups_);
}

bool Worker::RunTask() {
  auto task = GetNextTask();
  if (task) {
    TimePoint begin = TimePoint::Now();
    is_task_running = true;
    task->Run();
    is_task_running = false;
    for (auto &it : curr_group) {
      it->AddTime(TimePoint::Now() - begin);
    }
  } else if (is_terminated_) {
    return false;
  }
  return true;
}

void Worker::Run() {
  if (is_terminated_) {
    return;
  }
  local_worker_id = global_worker_id.fetch_add(1);
  RunLoop();
}

void Worker::SortNoLock() { // sort后优先级越高的TaskRunner分组距离队列头部越近
  if (!running_groups_.empty()) {
    running_groups_.sort([](const auto &lhs, const auto &rhs) {
      TDF_BASE_CHECK(!lhs.empty() && !rhs.empty());
      // 运行时间越短优先级越高，priority越小优先级越高
      int64_t left = lhs[0]->GetPriority() * lhs[0]->GetTime().ToNanoseconds();
      int64_t right = rhs[0]->GetPriority() * rhs[0]->GetTime().ToNanoseconds();
      if (left < right) {
        return true;
      }
      return false;
    });
  }
}

void Worker::Terminate() {
  TerminateWorker();
}

void Worker::BindGroup(int father_id, const std::shared_ptr<TaskRunner> &child) {
  std::lock_guard<std::mutex> running_lock(running_mutex_);
  std::list<std::vector<std::shared_ptr<TaskRunner>>>::iterator group_it;
  bool has_found = false;
  for (group_it = running_groups_.begin(); group_it != running_groups_.end(); ++group_it) {
    for (auto &runner_it : *group_it) {
      if (runner_it->GetId() == father_id) {
        has_found = true;
        break;
      }
    }
    if (has_found) {
      break;
    }
  }
  if (!has_found) {
    std::lock_guard<std::mutex> balance_lock(pending_mutex_);
    for (group_it = pending_groups_.begin(); group_it != pending_groups_.end(); ++group_it) {
      for (auto &runner_it : *group_it) {
        if (runner_it->GetId() == father_id) {
          has_found = true;
          break;
        }
      }
      if (has_found) {
        break;
      }
    }
  }
  group_it->push_back(child);
}

void Worker::Bind(std::vector<std::shared_ptr<TaskRunner>> group) {
  {
    std::lock_guard<std::mutex> lock(pending_mutex_);

    auto group_id = group[0]->GetGroupId();
    if (group_id != kDefaultGroupId) {
      group_id_ = group_id;
    }
    pending_groups_.insert(pending_groups_.end(), std::move(group));
  }
  need_balance_ = true;
  Notify();
}

void Worker::Bind(std::list<std::vector<std::shared_ptr<TaskRunner>>> list) {
  {
    std::lock_guard<std::mutex> lock(pending_mutex_);
    pending_groups_.splice(pending_groups_.end(), list);
  }
  need_balance_ = true;
  Notify();
}

void Worker::UnBind(const std::shared_ptr<TaskRunner> &runner) {
  std::vector<std::shared_ptr<TaskRunner>> group;
  bool has_found = false;
  {
    std::lock_guard<std::mutex> running_lock(running_mutex_);
    for (auto &running_group : running_groups_) {
      for (auto runner_it = running_group.begin(); runner_it != running_group.end(); ++runner_it) {
        if ((*runner_it)->GetId() == runner->GetId()) {
          running_group.erase(runner_it);
          has_found = true;
          break;
        }
      }
      if (has_found) {
        break;
      }
    }
  }

  if (!has_found) {
    {
      std::lock_guard<std::mutex> balance_lock(pending_mutex_);
      for (auto &pending_group : pending_groups_) {
        for (auto runner_it = pending_group.begin(); runner_it != pending_group.end();
             ++runner_it) {
          if ((*runner_it)->GetId() == runner->GetId()) {
            pending_group.erase(runner_it);
            has_found = true;
            break;
          }
        }
        if (has_found) {
          break;
        }
      }
    }
  }
}

int32_t Worker::GetRunningGroupSize() {
  std::lock_guard<std::mutex> lock(running_mutex_);
  return running_groups_.size();
}

std::list<std::vector<std::shared_ptr<TaskRunner>>> Worker::UnBind() {
  std::list<std::vector<std::shared_ptr<TaskRunner>>> ret;
  {
    std::lock_guard<std::mutex> lock(running_mutex_);
    ret.splice(ret.end(), running_groups_);
  }
  {
    std::lock_guard<std::mutex> lock(pending_mutex_);
    ret.splice(ret.end(), pending_groups_);
  }

  return ret;
}

std::list<std::vector<std::shared_ptr<TaskRunner>>> Worker::ReleasePending() {
  std::lock_guard<std::mutex> lock(pending_mutex_);

  std::list<std::vector<std::shared_ptr<TaskRunner>>> ret(std::move(pending_groups_));
  pending_groups_ = {};
  return ret;
}

std::list<std::vector<std::shared_ptr<TaskRunner>>> Worker::RetainActiveAndUnschedulable() {
  std::list<std::vector<std::shared_ptr<TaskRunner>>> ret;
  {
    std::lock_guard<std::mutex> lock(running_mutex_);
    for (auto group_it = running_groups_.begin(); group_it != running_groups_.end(); ++group_it) {
      if (group_it == running_groups_.begin()) {
        continue;
      }
      bool has_unschedulable = false;
      for (auto &runner : *group_it) {
        if (runner->IsSchedulable()) {
          has_unschedulable = true;
          break;
        }
      }
      if (has_unschedulable) {
        continue;
      }
      ret.splice(ret.end(), running_groups_, group_it);
    }
  }
  {
    std::lock_guard<std::mutex> lock(pending_mutex_);
    ret.splice(ret.end(), pending_groups_);
  }

  return ret;
}

std::list<std::vector<std::shared_ptr<TaskRunner>>> Worker::Retain(
    const std::shared_ptr<TaskRunner> &runner) {
  std::lock_guard<std::mutex> lock(running_mutex_);

  std::vector<std::shared_ptr<TaskRunner>> group;
  for (auto group_it = running_groups_.begin(); group_it != running_groups_.end(); ++group_it) {
    for (auto runner_it = group_it->begin(); runner_it != group_it->end(); ++runner_it) {
      if ((*runner_it)->GetId() == runner->GetId()) {
        group = *group_it;
        running_groups_.erase(group_it);
        break;
      }
    }
  }

  std::list<std::vector<std::shared_ptr<TaskRunner>>> ret(std::move(running_groups_));
  running_groups_ = {group};
  return ret;
}

void Worker::AddImmediateTask(std::unique_ptr<Task> task) {
  std::lock_guard<std::mutex> lock(running_mutex_);

  immediate_task_queue_.push(std::move(task));
}

template <class F>
auto MakeCopyable(F&& f) {
  auto s = std::make_shared<std::decay_t<F>>(std::forward<F>(f));
  return [s](auto&&... args) -> decltype(auto) {
    return (*s)(decltype(args)(args)...);
  };
}

std::unique_ptr<Task> Worker::GetNextTask() {
  {
    std::lock_guard<std::mutex> lock(running_mutex_);
    if (!immediate_task_queue_.empty()) {
      std::unique_ptr<Task> task = std::move(immediate_task_queue_.front());
      immediate_task_queue_.pop();
      return task;
    }
    if (running_groups_.size() > 1) {
      SortNoLock();
    }
  }

  if (need_balance_) {
    {
      std::scoped_lock lock(running_mutex_, pending_mutex_);
      BalanceNoLock();
    }
    need_balance_ = false;
  }

  TimeDelta last_wait_time;
  TimePoint now = TimePoint::Now();
  std::unique_ptr<IdleTask> idle_task;
  for (auto &running_group : running_groups_) {
    auto runner = running_group.back(); // group栈顶会阻塞下面的taskRunner执行
    auto task = runner->GetNext();
    if (task) {
      curr_group = running_group; // curr_group只会在当前线程获取，因此不需要加锁
      local_runner = runner;
      return task;
    } else {
      if (!idle_task) {
        idle_task = running_group.front()->PopIdleTask();
      }
      last_wait_time = running_group.front()->GetNextTimeDelta(now);
      if (min_wait_time_ > last_wait_time) {
        min_wait_time_ = last_wait_time;
        next_task_time_ = now + min_wait_time_;
      }
    }
  }
  if (!is_terminated_) {
    return nullptr;
  }
  if (idle_task) {
    auto wrapper_idle_task = std::make_unique<Task>(
        MakeCopyable([task = std::move(idle_task), time = min_wait_time_]() {
          IdleTask::IdleCbParam param = {
              .did_time_out =  false,
              .res_time =  time
          };
          task->Run(param);
        }));
    return wrapper_idle_task;
  }
  WaitFor(min_wait_time_);
  return nullptr;
}

bool Worker::IsTaskRunning() { return is_task_running; }

// 返回值小于0表示失败
int32_t Worker::WorkerKeyCreate(int32_t task_runner_id,
                                const std::function<void(void *)> &destruct) {
  auto map_it = worker_key_map_.find(task_runner_id);
  if (map_it == worker_key_map_.end()) {
    return -1;
  }
  auto array = map_it->second;
  for (auto i = 0; i < array.size(); ++i) {
    if (!array[i].is_used) {
      array[i].is_used = true;
      array[i].destruct = destruct;
      return i;
    }
  }
  return -1;
}

bool Worker::WorkerKeyDelete(int32_t task_runner_id, int32_t key) {
  auto map_it = worker_key_map_.find(task_runner_id);
  if (map_it == worker_key_map_.end()) {
    return false;
  }
  auto array = map_it->second;
  if (key >= array.size() || !array[key].is_used) {
    return false;
  }
  array[key].is_used = false;
  array[key].destruct = nullptr;
  return true;
}

bool Worker::WorkerSetSpecific(int32_t task_runner_id, int32_t key, void *p) {
  auto map_it = specific_map_.find(task_runner_id);
  if (map_it == specific_map_.end()) {
    return false;
  }
  auto array = map_it->second;
  if (key >= array.size()) {
    return false;
  }
  array[key] = p;
  return true;
}

void *Worker::WorkerGetSpecific(int32_t task_runner_id, int32_t key) {
  auto map_it = specific_map_.find(task_runner_id);
  if (map_it == specific_map_.end()) {
    return nullptr;
  }
  auto array = map_it->second;
  if (key >= array.size()) {
    return nullptr;
  }
  return array[key];
}

void Worker::WorkerDestroySpecific(int32_t task_runner_id) {
  WorkerDestroySpecificNoLock(task_runner_id);
}

void Worker::WorkerDestroySpecificNoLock(int32_t task_runner_id) {
  auto key_array_it = worker_key_map_.find(task_runner_id);
  auto specific_it = specific_map_.find(task_runner_id);
  if (key_array_it == worker_key_map_.end() || specific_it == specific_map_.end()) {
    return;
  }
  auto key_array = key_array_it->second;
  auto specific_array = specific_it->second;
  for (auto i = 0; i < specific_array.size(); ++i) {
    auto destruct = key_array[i].destruct;
    void *data = specific_array[i];
    if (destruct != nullptr && data != nullptr) {
      destruct(data);
    }
  }
  worker_key_map_.erase(key_array_it);
  specific_map_.erase(specific_it);
}

void Worker::WorkerDestroySpecifics() {
  for (auto map_it = specific_map_.begin(); map_it != specific_map_.end();) {
    auto next = ++map_it;
    Worker::WorkerDestroySpecificNoLock(map_it->first);
    map_it = next;
  }
}

std::array<Worker::WorkerKey, Worker::kWorkerKeysMax> Worker::GetMovedSpecificKeys(
    int32_t task_runner_id) {
  auto it = worker_key_map_.find(task_runner_id);
  if (it != worker_key_map_.end()) {
    auto ret = std::move(it->second);
    worker_key_map_.erase(it);
    return ret;
  }
  return std::array<Worker::WorkerKey, Worker::kWorkerKeysMax>();
}

void Worker::UpdateSpecificKeys(int32_t task_runner_id,
                                std::array<WorkerKey, Worker::kWorkerKeysMax> array) {
  worker_key_map_[task_runner_id] = std::move(array);  // insert or update
}

std::array<void *, Worker::kWorkerKeysMax> Worker::GetMovedSpecific(int32_t task_runner_id) {
  auto it = specific_map_.find(task_runner_id);
  if (it != specific_map_.end()) {
    auto ret = it->second;
    specific_map_.erase(it);
    return ret;
  }
  return std::array<void *, Worker::kWorkerKeysMax>();
}

void Worker::UpdateSpecific(int32_t task_runner_id,
                            std::array<void *, Worker::kWorkerKeysMax> array) {
  specific_map_[task_runner_id] = array;  // insert or update
}

} // namespace runner
} // namespace footstone
