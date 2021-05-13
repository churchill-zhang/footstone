/**
 * Copyright (c) 2020 Tencent Corporation. All rights reserved.
 * @brief 多生产者多消费者无锁队列实现（链表）
 * @details 基础数据结构，根据腾讯C++规范采用stl格式编写
 */

#pragma once

#include <cstdint>
#include <limits>

namespace tdf {
namespace detail {

template <std::size_t Bytes>
struct tagged_factor;

template <>
struct tagged_factor<4> {
  enum : std::uint64_t { mask = 0x00000000ffffffffull, incr = 0x0000000100000000ull };
};

template <>
struct tagged_factor<8> {
  enum : std::uint64_t { mask = 0x0000ffffffffffffull, incr = 0x0001000000000000ull };
};

template <typename T, std::size_t Bytes = sizeof(T)>
class tagged {
  std::uint64_t data_{0};

 public:
  enum : std::uint64_t { mask = tagged_factor<Bytes>::mask, incr = tagged_factor<Bytes>::incr };

  tagged() = default;
  tagged(tagged const&) = default;

  explicit tagged(std::uint64_t num) : data_(num) {}

  explicit tagged(T ptr) : data_(reinterpret_cast<std::uint64_t>(ptr)) {}

  tagged(T ptr, std::uint64_t tag) : data_(reinterpret_cast<std::uint64_t>(ptr) | (tag & ~mask)) {}

  tagged& operator=(tagged const&) = default;

  friend bool operator==(tagged a, tagged b) { return a.data_ == b.data_; }
  friend bool operator!=(tagged a, tagged b) { return !(a == b); }

  static std::uint64_t add(std::uint64_t tag) { return tag + incr; }
  static std::uint64_t del(std::uint64_t tag) { return tag - incr; }

  explicit operator T() const {
    auto ret = data_ & mask;
    return *reinterpret_cast<T*>(&ret);
  }

  std::uint64_t data() const { return data_; }

  T ptr() const { return static_cast<T>(*this); }

  T operator->() const { return static_cast<T>(*this); }
  auto operator*() const { return *static_cast<T>(*this); }
};

template <typename F>
class scope_exit {
  F f_;

 public:
  explicit scope_exit(F f) : f_(f) {}
  ~scope_exit() { f_(); }
};

}  // namespace detail

template <typename T>
class tagged {
 public:
  using dt_t = detail::tagged<T>;

 private:
  std::atomic<std::uint64_t> data_{0};

  bool compare_exchange(bool (std::atomic<std::uint64_t>::*cas)(std::uint64_t&, std::uint64_t,
                                                                std::memory_order),
                        dt_t& exp, T val, std::memory_order order) {
    auto num = exp.data();
    auto guard = detail::scope_exit{[&] { exp = num; }};
    return (data_.*cas)(num, dt_t{val, dt_t::add(num)}.data(), order);
  }

 public:
  tagged() = default;
  tagged(tagged const&) = default;

  explicit tagged(T ptr) : data_(reinterpret_cast<std::uint64_t>(ptr)) {}

  tagged& operator=(tagged const&) = default;

  auto load(std::memory_order order) const { return static_cast<T>(dt_t{data_.load(order)}); }

  auto tag_load(std::memory_order order) const { return dt_t{data_.load(order)}; }

  auto exchange(T val, std::memory_order order) {
    auto old = this->tag_load(std::memory_order_relaxed);
    while (!this->compare_exchange_weak(old, val, order)) {
    }
    return old;
  }

  void store(T val, std::memory_order order) { this->exchange(val, order); }

  bool compare_exchange_weak(dt_t& exp, T val, std::memory_order order) {
    return compare_exchange(&std::atomic<std::uint64_t>::compare_exchange_weak, exp, val, order);
  }

  bool compare_exchange_strong(dt_t& exp, T val, std::memory_order order) {
    return compare_exchange(&std::atomic<std::uint64_t>::compare_exchange_strong, exp, val, order);
  }
};

template <typename T>
class pool {
  union node {
    T data_;
    tagged<node*> next_;

    template <typename... P>
    node(P&&... pars) : data_{std::forward<P>(pars)...} {}
  };

  tagged<node*> cursor_{nullptr};
  std::atomic<node*> el_{nullptr};

 public:
  ~pool() {
    auto curr = cursor_.load(std::memory_order_relaxed);
    while (curr != nullptr) {
      auto temp = curr->next_.load(std::memory_order_relaxed);
      delete curr;
      curr = temp;
    }
  }

  bool empty() const { return cursor_.load(std::memory_order_acquire) == nullptr; }

  template <typename... P>
  T* alloc(P&&... pars) {
    typename tagged<node*>::dt_t curr = el_.exchange(nullptr, std::memory_order_relaxed);
    if (curr.ptr() == nullptr) {
      curr = cursor_.tag_load(std::memory_order_acquire);
      while (1) {
        if (curr.ptr() == nullptr) {
          return &((new node{std::forward<P>(pars)...})->data_);
        }
        auto next = curr->next_.load(std::memory_order_relaxed);
        if (cursor_.compare_exchange_weak(curr, next, std::memory_order_acquire)) {
          break;
        }
      }
    }
    return ::new (&(curr->data_)) T{std::forward<P>(pars)...};
  }

  void free(void* p) {
    if (p == nullptr) return;
    auto temp = reinterpret_cast<node*>(p);
    temp = el_.exchange(temp, std::memory_order_relaxed);
    if (temp == nullptr) {
      return;
    }
    auto curr = cursor_.tag_load(std::memory_order_relaxed);
    while (1) {
      temp->next_.store(curr.ptr(), std::memory_order_relaxed);
      if (cursor_.compare_exchange_weak(curr, temp, std::memory_order_release)) {
        break;
      }
    }
  }
};
/**
 * @brief 多生产者多消费者无锁队列实现（链表）
 * 长度没有上限限制，相比环实现内存占用更低，性能稍差
 */
template <typename T>
class mpmc_list {
  struct node {
    T data_;
    tagged<node*> next_;
  };

  pool<node> allocator_;

  tagged<node*> head_{allocator_.alloc()};
  tagged<node*> tail_{head_.load(std::memory_order_relaxed)};

 public:
  /**
   * @brief 判断队列是否为空
   * @return 队列是否为空
   *     @retval true 队列为空
   *     @retval false 队列不为空
   */
  bool empty() const {
    return head_.load(std::memory_order_acquire)->next_.load(std::memory_order_relaxed) == nullptr;
  }

  /**
   * @brief 将一个元素插入队列尾部
   * @param val 要插入的元素
   * @return 插入是否成功
   *     @retval true 插入成功
   *     @retval false 插入不成功
   * @note 目前实现，插入将总是成功
   */
  bool push(T const& val) {
    auto p = allocator_.alloc(val, nullptr);
    auto tail = tail_.tag_load(std::memory_order_relaxed);
    while (1) {
      auto next = tail->next_.tag_load(std::memory_order_acquire);
      if (tail == tail_.tag_load(std::memory_order_relaxed)) {
        if (next.ptr() == nullptr) {
          if (tail->next_.compare_exchange_weak(next, p, std::memory_order_relaxed)) {
            tail_.compare_exchange_strong(tail, p, std::memory_order_release);
            break;
          }
        } else if (!tail_.compare_exchange_weak(tail, next.ptr(), std::memory_order_relaxed)) {
          continue;
        }
      }
      tail = tail_.tag_load(std::memory_order_relaxed);
    }
    return true;
  }

  /**
   * @brief 将队列头部的元素出栈
   * @param val 出栈元素的输出
   * @return 出栈是否成功
   *     @retval true 成功
   *     @retval false 不成功（队列为空）
   */
  bool pop(T& val) {
    auto head = head_.tag_load(std::memory_order_acquire);
    auto tail = tail_.tag_load(std::memory_order_acquire);
    while (1) {
      auto next = head->next_.load(std::memory_order_acquire);
      if (head == head_.tag_load(std::memory_order_relaxed)) {
        if (head.ptr() == tail.ptr()) {
          if (next == nullptr) {
            return false;
          }
          tail_.compare_exchange_weak(tail, next, std::memory_order_relaxed);
        } else {
          val = next->data_;
          if (head_.compare_exchange_weak(head, next, std::memory_order_acquire)) {
            allocator_.free(head.ptr());
            return true;
          }
          tail = tail_.tag_load(std::memory_order_acquire);
          continue;
        }
      }
      head = head_.tag_load(std::memory_order_acquire);
      tail = tail_.tag_load(std::memory_order_acquire);
    }
  }
};

}  // namespace tdf