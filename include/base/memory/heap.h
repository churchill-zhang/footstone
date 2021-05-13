// Copyright (c) 2020 Tencent Corporation. All rights reserved.
#pragma once
#include <functional>
#include <memory>
#include "meta.h"
#include "memory_monitor.h"

#ifndef ALLOCATE_WITH_META
#define ALLOCATE_WITH_META 0
#else
#define ALLOCATE_WITH_META 1
#endif

namespace tdf {
namespace base {
#if ALLOCATE_WITH_META
const int kHeapMetaSize = sizeof(base::HeapMeta);
#else
const int kHeapMetaSize = 0;
#endif

/**
 * @brief       从一个对象的内存地址获得其HeapMeta地址
 */
inline HeapMeta *MetaOf(void *obj) {
  return reinterpret_cast<HeapMeta *>(reinterpret_cast<char *>(obj) - kHeapMetaSize);
}

/**
 * @brief       初始化一个并跟踪对象的HeapMeta结构
 */
template<class _Tp>
inline void ConstructAndWatchMeta(_Tp *obj, const char *file, int line, const char *type) {
  HeapMeta *meta = new(MetaOf(obj)) HeapMeta;
  meta->next = nullptr;
  meta->prev = nullptr;
  meta->data.type = type;
  meta->data.file = file;
  meta->data.line = line;
  meta->data.size = sizeof(_Tp);
  MemoryMonitor::Watch(meta);
}

/**
 * @brief       智能指针deleter
 */
template<class _Tp>
struct Deleter {
  static_assert(!std::is_function<_Tp>::value,
                "Deleter cannot be instantiated for std::function types");
#ifndef _LIBCPP_CXX03_LANG
  constexpr Deleter() _NOEXCEPT = default;
#else
  Deleter() {}
#endif
  template<class _Up>
  Deleter(
      const Deleter<_Up> &,
      typename std::enable_if<std::is_convertible<_Up *, _Tp *>::value>::type * = 0) _NOEXCEPT {}

  void operator()(_Tp *__ptr) const _NOEXCEPT {
    static_assert(sizeof(_Tp) > 0, "Deleter can not delete incomplete type");
    static_assert(!std::is_void<_Tp>::value, "Deleter can not delete incomplete type");

    char *ptr = reinterpret_cast<char *>(__ptr) - kHeapMetaSize;

#if ALLOCATE_WITH_META
    HeapMeta *meta_ptr = reinterpret_cast<HeapMeta *>(ptr);
    MemoryMonitor::UnWatch(meta_ptr);
    meta_ptr->~HeapMeta();
#endif

    __ptr->~_Tp();
    delete[] ptr;
  }
};

/**
 * @brief       使用Allocator分配的对象的智能指针deleter
 */
template<class _Tp, class _Alloc>
struct AllocatorDeleter {
  static_assert(!std::is_function<_Tp>::value,
                "AllocatorDeleter cannot be instantiated for std::function types");
  std::shared_ptr<_Alloc> allocator_;

#ifndef _LIBCPP_CXX03_LANG
  explicit constexpr AllocatorDeleter(std::shared_ptr<_Alloc> allocator) _NOEXCEPT
      : allocator_(allocator) {}
#else
  explicit AllocatorDeleter(_Allocator &allocator) : allocator_(allocator) {}
#endif

  void operator()(_Tp *__ptr) const _NOEXCEPT {
    static_assert(sizeof(_Tp) > 0, "AllocatorDeleter can not delete incomplete type");
    static_assert(!std::is_void<_Tp>::value, "AllocatorDeleter can not delete incomplete type");

#if ALLOCATE_WITH_META
    char *ptr = reinterpret_cast<char *>(__ptr) - kHeapMetaSize;
    HeapMeta *meta_ptr = reinterpret_cast<HeapMeta *>(ptr);
    MemoryMonitor::UnWatch(meta_ptr);
    meta_ptr->~HeapMeta();
#endif

    __ptr->~_Tp();
    allocator_->Deallocate(__ptr);
  }
};

/**
 * @brief       使用place holder new初始化对象
 */
template<class _Tp, typename ..._Args>
inline void Construct(_Tp *obj, _Args &&...__args) {
  new(obj) _Tp(std::forward<_Args>(__args)...);
}

#define ALLOC_SIZE_OF(type) kHeapMetaSize + sizeof(type)

/**
 * @brief       Allocator基类
 */
template<class _Tp>
class Allocator {
 public:
  virtual std::shared_ptr<_Tp> Allocate() = 0;
  virtual void Deallocate(_Tp *ptr) = 0;

  template<class _Up, typename ..._Args>
  inline void Construct(_Up *__p, _Args &&... __args) {
    base::Construct(__p, std::forward<_Args>(__args)...);
  }
};

/**
 * @brief       分配堆对象，根据宏开关决定是否在头部多分配一个HeapMeta结构
 * @return
 */
template<class _Tp, typename ..._Args>
inline _Tp *Alloc(_Args &&... __args) {
  auto *obj = reinterpret_cast<_Tp *>(new char[kHeapMetaSize + sizeof(_Tp)] + kHeapMetaSize);

  Construct(obj, std::forward<_Args>(__args)...);

  return obj;
}

/**
 * @brief       相比std::make_shared增加了HeapMeta跟踪
 */
template<class _Tp, typename ..._Args>
typename std::enable_if<!std::is_array<_Tp>::value, std::shared_ptr<_Tp>>::type
MakeShared(const char *file, int line, const char *type, _Args &&...__args) {
  auto obj = Alloc<_Tp>(std::forward<_Args>(__args)...);
#if ALLOCATE_WITH_META
  ConstructAndWatchMeta(obj, file, line, type);
#endif
  return std::shared_ptr<_Tp>(obj, Deleter<_Tp>());
}

/**
 * @brief       相比std::allocate_shared增加了HeapMeta跟踪
 */
template<class _Tp, class _Alloc, typename ..._Args>
typename std::enable_if<!std::is_array<_Tp>::value, std::shared_ptr<_Tp>>::type
AllocateShared(std::shared_ptr<_Alloc> __a, const char *file, int line,
    const char *type, _Args &&...__args) {
  assert(__a != nullptr);
  auto obj = __a->Allocate();
  __a->Construct(obj.get(), std::forward<_Args>(__args)...);
#if ALLOCATE_WITH_META
  ConstructAndWatchMeta(obj.get(), file, line, type);
  MetaOf(obj.get())->allocator_ = __a.get();
#endif

  return obj;
}

/**
 * @brief       相比std::make_unique增加了HeapMeta跟踪
 */
template<class _Tp, typename ..._Args>
typename std::enable_if<!std::is_array<_Tp>::value, std::unique_ptr<_Tp, Deleter<_Tp>>>::type
MakeUnique(const char *file, int line, const char *type, _Args &&...__args) {
  auto obj = Alloc<_Tp>(std::forward<_Args>(__args)...);
#if ALLOCATE_WITH_META
  ConstructAndWatchMeta(obj, file, line, type);
#endif

  return std::unique_ptr<_Tp, Deleter<_Tp>>(obj);
}
}  // namespace base
}  // namespace tdf

#define TDF_MAKE_SHARED(type, ...) \
  tdf::base::MakeShared<type>(__FILE__, __LINE__, #type, ##__VA_ARGS__)
#define TDF_MAKE_UNIQUE(type, ...) \
  tdf::base::MakeUnique<type>(__FILE__, __LINE__, #type, ##__VA_ARGS__)

#define TDF_ALLOCATE_SHARED(allocator, type, ...) \
base::AllocateShared<type>(allocator, __FILE__, __LINE__, #type, ##__VA_ARGS__)

#define TDF_UNIQUE_PTR(type) std::unique_ptr<type, tdf::base::Deleter<type>>
#define DEFINE_UNIQUE_PTR(type) using UniquePtr = std::unique_ptr<type, base::Deleter<type>>

/**
 * @brief       将base::tdf_construct加为友元，使得其能通过place holder new调用protected构造函数
 */
#define FRIEND_OF_TDF_ALLOC            \
  template <class _Tp, typename ..._Args> \
  friend inline void tdf::base::Construct(_Tp *obj, _Args &&...__args)
