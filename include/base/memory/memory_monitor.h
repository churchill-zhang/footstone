// Copyright (c) 2020 Tencent Corporation. All rights reserved.
//
// Created by willisdai on 2021/1/28.
//
#pragma once
#include <mutex>
#include <set>
#include <vector>
#include "meta.h"

namespace tdf {
namespace base {
struct MonitorHeapMeta {
  HeapMetaData data;
  void *address;
};

/**
   * @brief     MonitorHeapMeta转json
   */
std::string MonitorHeapMetaToJson(const std::shared_ptr<std::vector<base::MonitorHeapMeta>>& meta);

/**
 * @brief       跟踪堆对象的HeapMeta结构，DevTools可在运行时获取所有堆对象的基本信息
 */
class MemoryMonitor {
 public:
  /**
   * @brief     跟踪一个HeapMeta，在初始化智能指针分配对象分配时调用
   * @param     meta
   */
  static void Watch(HeapMeta *meta);

  /**
   * @brief     删除一个HeapMeta，在智能指针deleter中调用
   * @param     meta
   */
  static void UnWatch(HeapMeta *meta);

  /**
   * @brief     收集所有堆对象的HeapMeta信息，转换成MonitorHeapMeta结构数组返回
   */
  static std::shared_ptr<std::vector<MonitorHeapMeta>> CollectAllHeapMeta();

  static std::mutex mutex_;
  static HeapMeta dummy_head_;
  static HeapMeta *tail_;
  static size_t meta_count_;
};
}  // namespace base
}  // namespace tdf
