// Copyright (c) 2020 Tencent Corporation. All rights reserved.

#define BASE_USED_ON_EMBEDDER

#include "base/thread.h"

#include <windows.h>

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

// The information on how to set the thread name comes from
// a MSDN article: http://msdn2.microsoft.com/en-us/library/xcb2z8hs.aspx
const DWORD kVCThreadNameException = 0x406D1388;
typedef struct tagTHREADNAME_INFO {
  DWORD dwType;      // Must be 0x1000.
  LPCSTR szName;     // Pointer to name (in user addr space).
  DWORD dwThreadID;  // Thread ID (-1=caller thread).
  DWORD dwFlags;     // Reserved for future use, must be zero.
} THREADNAME_INFO;

void Thread::SetCurrentThreadName(const std::string& name) {
  if (name == "") {
    return;
  }

  THREADNAME_INFO info;
  info.dwType = 0x1000;
  info.szName = name.c_str();
  info.dwThreadID = GetCurrentThreadId();
  info.dwFlags = 0;
  __try {
    RaiseException(kVCThreadNameException, 0, sizeof(info) / sizeof(DWORD),
                   reinterpret_cast<DWORD_PTR*>(&info));
  } __except (EXCEPTION_CONTINUE_EXECUTION) {  // NOLINT
  }
}
}  // namespace base
}  // namespace tdf