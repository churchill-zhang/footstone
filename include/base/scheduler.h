//
// Created by dexterzhao on 2020/7/28.
// Copyright (c) 2020 Tencent Corporation. All rights reserved.
//

#pragma once
#include <memory>
#include "task_runner.h"

namespace tdf {
namespace base {

class Scheduler {
 public:
  virtual void Resize(int64_t size) = 0;
  virtual std::shared_ptr<TaskRunner> CreateTaskRunner(std::string label, bool is_excl,
                                                       int64_t priority) = 0;
};

}  // namespace base
}  // namespace tdf
