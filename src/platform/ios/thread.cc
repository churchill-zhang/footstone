// Copyright (c) 2020 Tencent Corporation. All rights reserved.

#define BASE_USED_ON_EMBEDDER

#include "base/thread.h"

#include <pthread.h>

#include <string>
#include <thread>

namespace tdf {
namespace base {

Thread::Thread(const std::string& name) {
  thread_ = std::make_unique<std::thread>([this, name]() mutable -> void {
    SetCurrentThreadName(name);
    Run();
  });
}

Thread::~Thread() {}

void Thread::Join() { thread_->join(); }

void Thread::SetCurrentThreadName(const std::string& name) {
  if (name == "") {
    return;
  }

  pthread_setname_np(name.c_str());
}
}  // namespace base
}  // namespace tdf
