/**
 * Copyright (c) 2020 Tencent Corporation. All rights reserved.
 * @brief 单生产者单消费者无锁队列实现（环队列）
 * @details 基础数据结构，根据腾讯C++规范采用stl格式编写
 */
#pragma once

#include <atomic>
#include <cstdint>
#include <limits>
#include <new>
#include <tuple>
#include <utility>

namespace tdf {
/**
 * @brief 单生产者单消费者无锁队列（环队列）
 * @param T 队列元素的类型
 * @param U 索引的类型（影响队列最大容量）
 */
template <typename T, typename U = uint8_t>
class spsc_queue {
 public:
  static constexpr U size_limit = std::numeric_limits<U>::max();

    /**
   * @brief 构造函数
   * @param max_size 队列最大容量
   * @note 队列最大容量无法超过索引类型的上限，即numeric_limits<U>::max()
   */
  explicit spsc_queue(size_t max_size = size_limit) : max_size_(max_size) {}

  // Return true if queue is empty.
  bool empty() const {
    return tail_.load(std::memory_order_relaxed) == head_.load(std::memory_order_acquire);
  }

  /**
   * @brief 添加元素到队列头部
   * @param val 要添加的元素
   *  @return 添加元素是否成功
   *     @retval true 成功
   *     @retval false 失败（由于达到队列上限）
   */
  bool push(T const& val) {
    auto id_head = head_.load(std::memory_order_relaxed);
    auto id_tail = tail_.load(std::memory_order_acquire);
    if (id_head == id_tail - 1) {
      return false;  // Queue is full because of size_limit acording to numeric_limits<U>::max().
    }
    if (static_cast<U>(id_head - id_tail) >= max_size_) {
      return false;  // Queue is full because of max_size.
    }
    block_[id_head] = val;
    head_.fetch_add(1, std::memory_order_release);
    return true;
  }

  /**
   * @brief 从队列尾部取出元素
   * @param val 被取出的元素引用
   *  @return 取出元素是否成功
   *     @retval true 成功
   *     @retval false 失败（由于队列为空）
   */
  bool pop(T& val) {
    auto id_tail = tail_.load(std::memory_order_relaxed);
    if (id_tail == head_.load(std::memory_order_acquire)) {
      return false;  // Queue is empty, pop nothing and return false.
    }
    val = block_[id_tail];
    tail_.fetch_add(1, std::memory_order_release);
    return true;
  }

  /**
   * @brief 添加元素到队列中，如果已满则移除队尾元素，保证最终一定添加成功
   * @param val 添加到队列中的元素
   */
  void push_until_success(T const& val) {
    while (!push(val)) {
      T popedVal;
      pop(popedVal);
    }
  }

 protected:
  size_t max_size_;
  T block_[size_limit + 1];

  // Tail index for reading.
  std::atomic<U> tail_{0};

  // Head index for writing.
  std::atomic<U> head_{0};
};

}  // namespace tdf
