// Copyright (c) 2020 Tencent Corporation. All rights reserved.

#ifndef TDF_BASE_INCLUDE_PROFILE_H_
#define TDF_BASE_INCLUDE_PROFILE_H_

#include <memory>
#include "task.h"

namespace footstone {
namespace base {
class Profile {
 public:
  virtual std::unique_ptr<Task> GetNextTask() = 0;
};
}  // namespace base
}  // namespace footstone

#endif  // TDF_BASE_INCLUDE_PROFILE_H_
