//
// Copyright (c) Tencent Corporation. All rights reserved.
//

#pragma once
#include "../../../../../Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX11.3.sdk/usr/include/c++/v1/atomic"
#include "../../../../../Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX11.3.sdk/usr/include/c++/v1/future"
#include "../../../../../Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX11.3.sdk/usr/include/c++/v1/memory"
#include "../../../../../Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX11.3.sdk/usr/include/c++/v1/string"
#include "../../../../../Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX11.3.sdk/usr/include/c++/v1/thread"

#include "macros.h"

namespace footstone {
inline namespace runner {

class Thread {
 public:
  explicit Thread(const std::string& name = "");

  ~Thread();

  void Start();

  void Join();

  static void SetCurrentThreadName(const std::string& name);

  virtual void Run() = 0;

 private:
  std::string name_;
  std::unique_ptr<std::thread> thread_;

  TDF_BASE_DISALLOW_COPY_AND_ASSIGN(Thread);
};

}  // namespace runner
}  // namespace footstone
