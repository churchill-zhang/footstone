/**
 * Copyright (c) 2020 Tencent Corporation. All rights reserved.
 * @brief 多生产者多消费者无锁队列实现（环队列）
 * @details 基础数据结构，根据腾讯C++规范采用stl格式编写
 */
#pragma once

#include <atomic>
#include <cstdint>
#include <limits>
#include <new>
#include <thread>
#include <tuple>
#include <utility>

#include "spsc_queue.h"

namespace footstone {

constexpr uint64_t invalid_index = std::numeric_limits<std::uint64_t>::max();

/**
 * @brief 队列节点
 * @param T 队列元素的类型
 */
template <typename T>
struct rnode {
  T data;
  std::atomic<uint64_t> flag{invalid_index};
};

/**
 * @brief 多生产者多消费者无锁队列（环队列）
 * @param T 队列元素的类型
 * @param U 索引的类型（影响队列最大容量）
 */
template <typename T, typename U = uint8_t>
class mpmc_queue : public spsc_queue<rnode<T>, U> {
  using super = spsc_queue<rnode<T>, U>;
  using super::size_limit;

 public:
  /**
   * @brief 构造函数
   * @param max_size 队列最大容量
   * @note 队列最大容量无法超过索引类型的上限，即numeric_limits<U>::max()
   */
  explicit mpmc_queue(size_t max_size = size_limit) : super(max_size) {}

  /**
   * @brief 添加元素到队列头部
   * @param val 要添加的元素
   *  @return 添加元素是否成功
   *     @retval true 成功
   *     @retval false 失败（由于达到队列上限）
   */
  bool push(T const& val) {
    U cur_flag = flag_.load(std::memory_order_acquire), next_flag;
    while (1) {
      next_flag = cur_flag + 1;
      auto id_tail = tail_.load(std::memory_order_acquire);
      if (next_flag == id_tail) {
        return false;  // Queue is full because of size_limit acording to numeric_limits<U>::max().
      }
      if (static_cast<U>(next_flag - id_tail) >= max_size_) {
        return false;  // Queue is full because of max_size.
      }

      if (flag_.compare_exchange_weak(cur_flag, next_flag, std::memory_order_acq_rel)) {
        break;
      }
    }
    auto* item = block_ + cur_flag;
    item->data = val;
    item->flag.store(cur_flag, std::memory_order_release);
    while (1) {
      auto cac_flag = item->flag.load(std::memory_order_acquire);
      if (cur_flag != head_.load(std::memory_order_acquire)) {
        return true;
      }
      if (cac_flag != cur_flag) {
        return true;
      }
      if (!item->flag.compare_exchange_strong(cac_flag, invalid_index, std::memory_order_relaxed)) {
        return true;
      }
      head_.store(next_flag, std::memory_order_release);
      cur_flag = next_flag;
      next_flag = cur_flag + 1;
      item = block_ + cur_flag;
    }
  }

  /**
   * @brief 从队列尾部取出元素
   * @param val 被取出的元素引用
   *  @return 取出元素是否成功
   *     @retval true 成功
   *     @retval false 失败（由于队列为空）
   */
  bool pop(T& val) {
    auto cur_tail = tail_.load(std::memory_order_relaxed);
    while (1) {
      auto id_tail = cur_tail;
      auto cur_head = head_.load(std::memory_order_acquire);
      if (id_tail == cur_head) {
        auto* item = block_ + cur_head;
        auto cac_flag = item->flag.load(std::memory_order_acquire);
        if (cac_flag != cur_head) {
          return false;  // Queue is empty, pop nothing and return false.
        }
        if (item->flag.compare_exchange_weak(cac_flag, size_limit, std::memory_order_relaxed)) {
          head_.store(cur_head + 1, std::memory_order_release);
        }
        cur_tail = tail_.load(std::memory_order_relaxed);
      } else {
        val = block_[id_tail].data;
        if (tail_.compare_exchange_weak(cur_tail, cur_tail + 1, std::memory_order_release)) {
          return true;
        }
      }
    }
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
  using super::block_;
  using super::head_;
  using super::max_size_;
  using super::tail_;
  std::atomic<U> flag_{0};
};

}  // namespace footstone
