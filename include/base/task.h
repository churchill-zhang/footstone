// Copyright (c) 2020 Tencent Corporation. All rights reserved.

#pragma once

#include <functional>
#include <future>

namespace tdf {
namespace base {
class Task {
 public:
  explicit Task(std::function<void()> func = nullptr) : is_canceled_(false), cb_(func) {}
  ~Task() = default;

  inline void Run() {
    if (cb_) {
      cb_();
    }
  }
  void Cancel() { is_canceled_ = true; }
  bool IsCanceled() { return is_canceled_; }

 protected:
  std::atomic<bool> is_canceled_;
  std::function<void()> cb_;
};

template <typename F, typename... Args>
class FutureTask : public Task {
 public:
  using T = typename std::result_of<F(Args...)>::type;

  FutureTask<T>(F&& f, Args&&... args) {
    task_ = std::make_shared<std::packaged_task<T()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    cb_ = [this]() { (*task_)(); };
  }

  std::future<T> Future() { return task_->get_future(); }

 private:
  std::shared_ptr<std::packaged_task<T()>> task_;
};

}  // namespace base
}  // namespace tdf
