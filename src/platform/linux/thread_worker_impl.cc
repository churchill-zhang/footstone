// Copyright (c) 2020 Tencent Corporation. All rights reserved.

#include "footstone/thread_worker_impl.h"

#include <pthread.h>

namespace footstone {
inline namespace runner {

void ThreadWorkerImpl::SetName(const std::string& name) {
  if (name == "") {
    return;
  }

  pthread_setname_np(pthread_self(), name.c_str());
}

} // namespace runner
} // namespace footstone
