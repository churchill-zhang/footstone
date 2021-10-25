// Copyright (c) 2020 Tencent Corporation. All rights reserved.

#pragma once

#include <memory>
#include "task.h"

namespace footstone {
inline namespace profile {

class Profile {
 public:
  virtual std::unique_ptr<Task> GetNextTask() = 0;
};

}  // namespace base
}  // namespace footstone
