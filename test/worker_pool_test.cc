// Copyright (c) 2020 Tencent Corporation. All rights reserved.

#include <future>

#include "base/logging.h"
#include "base/worker_pool.h"

using WorkerPool = footstone::WorkerPool;
using TaskRunner = footstone::TaskRunner;
using Task = footstone::Task;

int main() {
  std::shared_ptr<WorkerPool> pool = WorkerPool::GetInstance(3);

  std::this_thread::sleep_for(std::chrono::seconds(5));

  TDF_BASE_LOG(ERROR) << "Resize begin ";
  pool->Resize(5);
  TDF_BASE_LOG(ERROR) << "Resize ";
  pool->Resize(1);
  TDF_BASE_LOG(ERROR) << "Resize end ";

  std::this_thread::sleep_for(std::chrono::seconds(5));

  TDF_BASE_LOG(ERROR) << "CreateTestTaskRunner begin ";
  auto test = pool->CreateTaskRunner(true, 1, "testTaskRunner");
  test->PostTask([=]() {
      test->GetName();
  });
  TDF_BASE_LOG(ERROR) << "CreateTestTaskRunner end ";

  std::this_thread::sleep_for(std::chrono::seconds(100000000000000));

  return 0;
}
