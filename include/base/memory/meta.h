// Copyright (c) 2020 Tencent Corporation. All rights reserved.
//
// Created by willisdai on 2021/3/3.
//
#pragma once
#include <stdint.h>

namespace tdf::base {
/**
 * @brief       记录了堆对象的基本信息：类型、分配位置、大小、Allocator等
 */
struct HeapMetaData {
  const char *type;
  const char *file;
  int line;
  int64_t size;
  void *allocator = nullptr;
};

struct HeapMeta {
  HeapMetaData data;
  HeapMeta *next;
  HeapMeta *prev;
};
}  // namespace tdf::base