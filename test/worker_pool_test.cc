// Copyright (c) 2020 Tencent Corporation. All rights reserved.

#include <future>

#include "base/logging.h"
#include "base/worker_pool.h"

std::future<void> CreateTestTaskRunner(std::shared_ptr<WorkerPool> pool, int min, int max) {
  std::shared_ptr<TaskRunner> runner = pool->CreateTaskRunner("TestRunner");
  std::future<void> future = std::async(std::launch::async, [min, max, runner]() {
    for (int i = min; i < max; ++i) {
      std::unique_ptr<Task> task = std::make_unique<Task>();
      task->cb_ = [runner, i] {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        TDF_BASE_LOG(INFO) << "this is a test, runner id = " << runner->GetId() << ", i = " << i;
      };
      runner->PostTask(std::move(task));
    }
  });
  return future;
}

int main() {
  std::shared_ptr<WorkerPool> pool = WorkerPool::GetInstance(3);

  std::vector<std::future<void>> futures;
  for (int i = 0; i < 5; ++i) {
    futures.push_back(CreateTestTaskRunner(pool, (i + 1) * 100, (i + 2) * 100));
  }
  for (auto it = futures.begin(); it != futures.end(); ++it) {
    it->wait();
  }

  std::this_thread::sleep_for(std::chrono::seconds(5));

  TDF_BASE_LOG(ERROR) << "Resize begin ";
  pool->Resize(5);
  TDF_BASE_LOG(ERROR) << "Resize ";
  pool->Resize(1);
  TDF_BASE_LOG(ERROR) << "Resize end ";

  std::this_thread::sleep_for(std::chrono::seconds(5));

  TDF_BASE_LOG(ERROR) << "CreateTestTaskRunner begin ";
  CreateTestTaskRunner(pool, 6 * 100, 7 * 100).wait();
  TDF_BASE_LOG(ERROR) << "CreateTestTaskRunner end ";

  std::this_thread::sleep_for(std::chrono::seconds(100000000000000));

  return 0;
}
