#include "footstone/task_runner.h"

#include <atomic>
#include <array>
#include <utility>

#include "footstone/logging.h"
#include "footstone/worker.h"

namespace footstone {
inline namespace runner {

using IdleCbParam = IdleTask::IdleCbParam;

std::atomic<uint32_t> global_task_runner_id{0};

TaskRunner::TaskRunner(uint32_t group_id, uint32_t priority, bool is_schedulable, std::string name):
      group_id_(group_id),
      name_(std::move(name)),
      has_sub_runner_(false),
      is_schedulable_(is_schedulable),
      priority_(priority),
      time_(TimeDelta::Zero()) {
  id_ = global_task_runner_id.fetch_add(1);
}

TaskRunner::~TaskRunner() {
  std::shared_ptr<Worker> worker = worker_.lock();
  if (worker) {
    worker->WorkerDestroySpecific(id_);
  }
}

void TaskRunner::Clear() {
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    while (!task_queue_.empty()) {
      task_queue_.pop();
    }
  }
  {
    std::lock_guard<std::mutex> lock(delay_mutex_);
    while (!delayed_task_queue_.empty()) {
      delayed_task_queue_.pop();
    }
  }
  {
    std::lock_guard<std::mutex> lock(idle_mutex_);
    while (!idle_task_queue_.empty()) {
      delayed_task_queue_.pop();
    }
  }
}

bool TaskRunner::AddSubTaskRunner(const std::shared_ptr<TaskRunner>& sub_runner,
                                  bool is_task_running) {
  std::shared_ptr<Worker> worker = worker_.lock();
  if (!worker) {
    return false;
  }
  worker->BindGroup(id_, sub_runner);
  has_sub_runner_ = true;
  if (is_task_running) {
    worker->SetStackingMode(true);
    while (has_sub_runner_) {
      worker->RunTask();
    }
  }
  return true;
}

bool TaskRunner::RemoveSubTaskRunner(const std::shared_ptr<TaskRunner>& sub_runner) {
  if (!has_sub_runner_ || !sub_runner) {
    return false;
  }
  std::shared_ptr<Worker> worker = worker_.lock();
  if (!worker) {
    return false;
  }
  worker->UnBind(sub_runner);
  has_sub_runner_ = false;
  NotifyWorker();
  return true;
}

void TaskRunner::PostTask(std::unique_ptr<Task> task) {
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    task_queue_.push(std::move(task));
  }
  NotifyWorker();
}

void TaskRunner::PostDelayedTask(std::unique_ptr<Task> task, TimeDelta delay) {
  {
    std::lock_guard<std::mutex> lock(delay_mutex_);

    TimePoint deadline = TimePoint::Now() + delay;
    delayed_task_queue_.push(std::make_pair(deadline, std::move(task)));
  }
  NotifyWorker();
}

void TaskRunner::PostIdleTask(std::unique_ptr<IdleTask> task) {
  {
    std::lock_guard<std::mutex> lock(idle_mutex_);
    idle_task_queue_.push(std::move(task));
  }
  NotifyWorker();
}

std::unique_ptr<Task> TaskRunner::PopTask() {
  std::lock_guard<std::mutex> lock(queue_mutex_);

  if (!task_queue_.empty()) {
    std::unique_ptr<Task> result = std::move(task_queue_.front());
    task_queue_.pop();
    return result;
  }

  return nullptr;
}

std::unique_ptr<IdleTask> TaskRunner::PopIdleTask() {
  std::lock_guard<std::mutex> lock(idle_mutex_);

  if (!idle_task_queue_.empty()) {
    std::unique_ptr<IdleTask> result = std::move(idle_task_queue_.front());
    idle_task_queue_.pop();
    return result;
  }

  return nullptr;
}

std::unique_ptr<Task> TaskRunner::GetTopDelayTask() {
  std::lock_guard<std::mutex> lock(delay_mutex_);

  if (task_queue_.empty() && !delayed_task_queue_.empty()) {
    std::unique_ptr<Task> result =
        std::move(const_cast<DelayedEntry&>(delayed_task_queue_.top()).second);
    return result;
  }
  return nullptr;
}

std::unique_ptr<Task> TaskRunner::GetNext() {
  TimePoint now = TimePoint::Now();
  std::unique_ptr<Task> task = popTaskFromDelayedQueueNoLock(now);
  {
    std::scoped_lock lock(queue_mutex_, delay_mutex_);
    while (task) {
      task_queue_.push(std::move(task));
      task = popTaskFromDelayedQueueNoLock(now);
    }
  }
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    if (!task_queue_.empty()) {
      std::unique_ptr<Task> result = std::move(task_queue_.front());
      task_queue_.pop();
      return result;
    }
  }
  return nullptr;
}

TimeDelta TaskRunner::GetNextTimeDelta(TimePoint now) {
  std::unique_lock<std::mutex> lock(delay_mutex_);
  if (!delayed_task_queue_.empty()) {
    const DelayedEntry& delayed_task = delayed_task_queue_.top();
    return delayed_task.first - now;
  } else {
    return TimeDelta::Max();
  }
}

void TaskRunner::NotifyWorker() {
  std::shared_ptr<Worker> worker = worker_.lock();
  TDF_BASE_CHECK(worker);
  worker->Notify();
}

std::unique_ptr<Task> TaskRunner::popTaskFromDelayedQueueNoLock(TimePoint now) {
  if (delayed_task_queue_.empty()) {
    return nullptr;
  }

  const DelayedEntry& deadline_and_task = delayed_task_queue_.top();
  if (deadline_and_task.first > now) {
    return nullptr;
  }

  std::unique_ptr<Task> result = std::move(const_cast<DelayedEntry&>(deadline_and_task).second);
  delayed_task_queue_.pop();
  return result;
}

std::shared_ptr<TaskRunner> TaskRunner::GetCurrentTaskRunner() {
  return Worker::GetCurrentTaskRunner();
}

int32_t TaskRunner::RunnerKeyCreate(const std::function<void(void*)>& destruct) {
  TDF_BASE_CHECK(Worker::IsTaskRunning()) << "RunnerKeyCreate cannot be run outside of the task";
  auto task_runner_id = Worker::GetCurrentTaskRunner()->GetId();
  std::shared_ptr<Worker> worker = worker_.lock();
  TDF_BASE_CHECK(worker); // task??????????????????worker????????????
  return worker->WorkerKeyCreate(task_runner_id, destruct);
}

bool TaskRunner::RunnerKeyDelete(int32_t key) {
  TDF_BASE_CHECK(Worker::IsTaskRunning()) << "RunnerKeyDelete cannot be run outside of the task";
  auto task_runner_id = Worker::GetCurrentTaskRunner()->GetId();
  std::shared_ptr<Worker> worker = worker_.lock();
  TDF_BASE_CHECK(worker);
  return worker->WorkerKeyDelete(task_runner_id, key);
}

bool TaskRunner::RunnerSetSpecific(int32_t key, void* p) {
  TDF_BASE_CHECK(Worker::IsTaskRunning()) << "RunnerSetSpecific cannot be run outside of the task";
  auto task_runner_id = Worker::GetCurrentTaskRunner()->GetId();
  std::shared_ptr<Worker> worker = worker_.lock();
  TDF_BASE_CHECK(worker);
  return worker->WorkerSetSpecific(task_runner_id, key, p);
}

void* TaskRunner::RunnerGetSpecific(int32_t key) {
  TDF_BASE_CHECK(Worker::IsTaskRunning()) << "RunnerGetSpecific cannot be run outside of the task";
  auto task_runner_id = Worker::GetCurrentTaskRunner()->GetId();
  std::shared_ptr<Worker> worker = worker_.lock();
  TDF_BASE_CHECK(worker);
  return worker->WorkerGetSpecific(task_runner_id, key);
}

void TaskRunner::RunnerDestroySpecifics() {
  TDF_BASE_CHECK(Worker::IsTaskRunning()) << "RunnerDestroySpecifics cannot be run outside of the task";
  auto task_runner_id = Worker::GetCurrentTaskRunner()->GetId();
  std::shared_ptr<Worker> worker = worker_.lock();
  TDF_BASE_CHECK(worker);
  return worker->WorkerDestroySpecific(task_runner_id);
}

} // namespace runner
}  // namespace footstone
