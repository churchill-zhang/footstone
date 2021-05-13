// Copyright (c) 2020 Tencent Corporation. All rights reserved.
// Created by  nolantang on 2020/8/21.
//

#pragma once

#include <functional>
#include <list>
#include <memory>
#include <utility>

namespace tdf {
namespace base {

class NotifierListener {
 public:
  using VoidCallback = std::function<void()>;

  explicit NotifierListener(VoidCallback listener) : listener_(std::move(listener)) {}

  void InvokeListener();

  VoidCallback GetListener() { return listener_; }

 private:
  VoidCallback listener_;
};

class Listenable {
 public:
  virtual void AddListener(std::shared_ptr<NotifierListener> listener) = 0;
  virtual void RemoveListener(std::shared_ptr<NotifierListener> listener) = 0;
};

class ChangeNotifier : public Listenable {
 public:
  ~ChangeNotifier();
  void AddListener(std::shared_ptr<NotifierListener> listener) override;
  void RemoveListener(std::shared_ptr<NotifierListener> listener) override;

  bool HasListeners() { return !listeners_.empty(); }

  bool ContainsListener(const std::shared_ptr<NotifierListener>& listener);

  void NotifyListeners();

  virtual void Dispose();

 private:
  std::list<std::shared_ptr<NotifierListener>> listeners_;
};

template <typename T>
class ValueNotifier : public ChangeNotifier {
 public:
  explicit ValueNotifier(T value) : value_(value), ChangeNotifier() {}
  T GetValue() const { return value_; }

  void SetValue(T newValue) {
    if (value_ == newValue) {
      return;
    }
    value_ = newValue;
    NotifyListeners();
  }

 private:
  T value_;
};

}  // namespace base
}  // namespace tdf
