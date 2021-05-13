// Copyright (c) 2020 Tencent Corporation. All rights reserved.

#define BASE_USED_ON_EMBEDDER

#include "base/worker_pool.h"

#include "base/logging.h"

namespace tdf {
namespace base {

std::mutex WorkerPool::creation_mutex_;

std::shared_ptr<WorkerPool> WorkerPool::instance_ = nullptr;

WorkerPool::WorkerPool(int64_t size) : index_(0), size_(size) { CreateWorker(size); }

WorkerPool::~WorkerPool() {
  for (auto &worker : excl_workers_) {
    if (!worker->IsTerminated()) {
      worker->Terminate();
    }
  }
}

std::shared_ptr<WorkerPool> WorkerPool::GetInstance(int64_t size) {
  std::scoped_lock creation(creation_mutex_);
  if (!instance_) {
    instance_ = std::make_shared<WorkerPool>(size);
  }
  return instance_;
}

void WorkerPool::CreateWorker(int64_t size, bool is_excl) {
  for (int64_t i = 0; i < size; ++i) {
    if (is_excl) {
      excl_workers_.push_back(std::make_unique<Worker>(""));
    } else {
      workers_.push_back(std::make_unique<Worker>(""));
    }
  }
}

void WorkerPool::IncreaseThreads(int64_t new_size) {
  assert(new_size > size_);
  CreateWorker(new_size - size_);
  std::list<std::shared_ptr<TaskRunner>> runners;
  for (int64_t i = 0; i < size_; ++i) {
    runners.splice(runners.end(), workers_[i]->RetainFront());
  }
  index_ = size_;
  auto it = runners.begin();
  while (it != runners.end()) {
    // use rr
    workers_[index_]->Bind(*it);
    index_ == new_size - 1 ? 0 : ++index_;
    ++it;
  }
  size_ = new_size;
}

void WorkerPool::ReduceThreads(int64_t new_size) {
  assert(new_size < size_);
  if (index_ > new_size - 1) {
    index_ = 0;
  }

  for (int64_t i = size_ - 1; i > new_size - 1; --i) {
    // handle running runner on thread
    auto runners = workers_[i]->UnBind();
    auto it = runners.begin();
    while (it != runners.end()) {
      // use rr
      workers_[index_]->Bind(*it);
      index_ == new_size - 1 ? 0 : ++index_;
      ++it;
    }

    // pending_runners_ needs to be processed
    runners = workers_[i]->ReleasePending();
    it = runners.begin();
    while (it != runners.end()) {
      workers_[index_]->Bind(*it);
      index_ == new_size - 1 ? 0 : ++index_;
      ++it;
    }

    workers_[i]->Terminate();
    workers_.pop_back();
  }
  size_ = new_size;
}

void WorkerPool::Resize(int64_t size) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (size == size_) {
    return;
  } else if (size > size_) {
    IncreaseThreads(size);
  } else {
    ReduceThreads(size);
  }
}

std::shared_ptr<TaskRunner> WorkerPool::CreateTaskRunner(std::string label, bool is_excl,
                                                         int64_t priority) {
  std::shared_ptr<TaskRunner> task_runner = std::make_shared<TaskRunner>(label, is_excl, priority);
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (is_excl) {
      CreateWorker(1, true);
      auto it = excl_workers_.rbegin();
      (*it)->Bind(task_runner);
      task_runner->SetCv((*it)->cv_);
    } else {
      workers_[index_]->Bind(task_runner);
      task_runner->SetCv(workers_[index_]->cv_);
      index_ == size_ - 1 ? 0 : ++index_;
    }
  }
  return task_runner;
}
}  // namespace base
}  // namespace tdf
