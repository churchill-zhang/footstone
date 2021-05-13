// Copyright (c) 2020 The tencent Authors. All rights reserved.
#pragma once

#include <mutex>
#include <queue>
namespace tdf {
namespace core {

// 基础库，根据腾讯C++代码规范7.10，使用stl命名规范
template <class T>
class thread_safe_queue {
 public:
  explicit thread_safe_queue(size_t max_count = SIZE_T_MAX) : max_count_(max_count) {}
  ~thread_safe_queue() {}

  // Push a object to queue.
  // Return true if operation is successful.
  inline bool push(T const &obj) {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    if (queue_.size() >= max_count_) {
      return false;
    }
    queue_.emplace(obj);
    return true;
  }

  // Pop a object from queue.
  // Return true if operation is successful.
  inline bool pop(T &obj) {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    if (queue_.empty()) {
      return false;
    }
    obj = std::move(queue_.front());
    queue_.pop();
    return true;
  }

  // Return true if queue is empty.
  inline bool empty(void) { return queue_.empty(); }

  // Pop and consume one object.
  template <typename Functor>
  inline bool consume_one(Functor const &f) {
    T element;
    bool success = pop(element);
    if (success) {
      f(element);
    }

    return success;
  }

  // Pop and consume all objects.
  template <typename Functor>
  inline size_t consume_all(Functor const &f) {
    size_t element_count = 0;
    while (consume_one(f)) {
      element_count += 1;
    }

    return element_count;
  }

 private:
  std::queue<T> queue_;
  std::mutex queue_mutex_;

  size_t max_count_;
};

}  // namespace core
}  // namespace tdf