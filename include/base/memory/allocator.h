// Copyright (c) 2020 Tencent Corporation. All rights reserved.
//
// Created by willisdai on 2021/2/23.
//
#pragma once

#include <list>
#include "heap.h"

namespace tdf {
namespace base {
template<class _Tp>
class DefaultAllocator
    : public Allocator<_Tp>, public std::enable_shared_from_this<DefaultAllocator<_Tp>> {

 public:
  std::shared_ptr<_Tp> Allocate() override {
    auto obj = reinterpret_cast<_Tp *>(new char[ALLOC_SIZE_OF(_Tp)] + kHeapMetaSize);
    return std::shared_ptr<_Tp>(obj,
        AllocatorDeleter< _Tp, DefaultAllocator>(this->shared_from_this()));
  }

  void Deallocate(_Tp *ptr) override {
    delete[] (reinterpret_cast<char *>(ptr) - kHeapMetaSize);
  }
};

template<class _Tp>
class CachedAllocator
    : public Allocator<_Tp>, public std::enable_shared_from_this<CachedAllocator<_Tp>> {
 public:
  std::shared_ptr<_Tp> Allocate() override {
    _Tp *obj = FetchFromCache();
    if (obj == nullptr) {
      obj = reinterpret_cast<_Tp *>(new char[ALLOC_SIZE_OF(_Tp)] + kHeapMetaSize);
    }
    return std::shared_ptr<_Tp>(obj,
        AllocatorDeleter<_Tp, CachedAllocator>(this->shared_from_this()));
  }

  void Deallocate(_Tp *obj) override {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.push_back(obj);
  }

 private:
  _Tp *FetchFromCache() {
    _Tp *obj = nullptr;
    std::lock_guard<std::mutex> lock(mutex_);
    if (!cache_.empty()) {
      obj = cache_.back();
      cache_.pop_back();
    }
    return obj;
  }

  std::mutex mutex_;
  std::list<_Tp*> cache_;
};
}  // namespace base
}  // namespace tdf