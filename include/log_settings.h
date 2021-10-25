// Copyright (c) 2020 Tencent Corporation. All rights reserved.

#ifndef TDF_BASE_INCLUDE_LOG_SETTINGS_H_
#define TDF_BASE_INCLUDE_LOG_SETTINGS_H_

#include "log_level.h"

#include "../../../../../Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX11.3.sdk/usr/include/c++/v1/string"

namespace footstone {
inline namespace log {

struct LogSettings {
  LogSeverity min_log_level = TDF_LOG_INFO;
};

void SetLogSettings(const LogSettings& settings);

LogSettings GetLogSettings();

int GetMinLogLevel();

}  // namespace log
}  // namespace footstone

#endif  // TDF_BASE_INCLUDE_LOG_SETTINGS_H_
