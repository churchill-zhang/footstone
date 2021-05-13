// Copyright (c) 2020 Tencent Corporation. All rights reserved.
// Created by  nolantang on 2020/8/21.
//

#include "base/change_notifier.h"

#include <iostream>

namespace tdf {
namespace base {

void NotifierListener::InvokeListener() { listener_(); }

ChangeNotifier::~ChangeNotifier() { listeners_.clear(); }

void ChangeNotifier::AddListener(std::shared_ptr<NotifierListener> listener) {
  listeners_.push_back(listener);
}

void ChangeNotifier::RemoveListener(std::shared_ptr<NotifierListener> listener) {
  if (ContainsListener(listener)) {
    listeners_.remove(listener);
  }
}

bool ChangeNotifier::ContainsListener(const std::shared_ptr<NotifierListener>& listener) {
  if (listeners_.empty()) {
    return false;
  }

  for (auto& tempListener : listeners_) {
    if (tempListener.get() == listener.get()) {
      return true;
    }
  }

  return false;
}

void ChangeNotifier::NotifyListeners() {
  if (listeners_.empty()) {
    return;
  }

  for (auto& listener : listeners_) {
    // 调用VoidCallback
    listener->InvokeListener();
  }
}

void ChangeNotifier::Dispose() {
  if (!listeners_.empty()) {
    listeners_.clear();
  }
}

}  // namespace base
}  // namespace tdf
