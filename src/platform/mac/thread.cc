// Copyright (c) 2020 Tencent Corporation. All rights reserved.

#define BASE_USED_ON_EMBEDDER

#include "thread.h"

#include <pthread.h>

#include <string>
#include <thread>

namespace footstone {
inline namespace runner {

Thread::Thread(const std::string& name) : name_(name) {}

void Thread::Start() {
  thread_ = std::make_unique<std::thread>([this]() -> void {
    SetCurrentThreadName(name_);
    Run();
  });
}

Thread::~Thread() {}

void Thread::Join() {
  if (thread_->joinable()) {
    thread_->join();
  }
}

void Thread::SetCurrentThreadName(const std::string& name) {
  if (name == "") {
    return;
  }

  pthread_setname_np(name.c_str());
}

} // namespace runner
} // namespace footstone
