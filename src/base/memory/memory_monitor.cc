// Copyright (c) 2020 Tencent Corporation. All rights reserved.
//
// Created by willisdai on 2021/1/28.
//
#include "base/memory/memory_monitor.h"
#include <sstream>
#include "base/memory/heap.h"

namespace tdf {
namespace base {
std::mutex MemoryMonitor::mutex_;
HeapMeta MemoryMonitor::dummy_head_ = {0};
HeapMeta *MemoryMonitor::tail_ = &MemoryMonitor::dummy_head_;
size_t MemoryMonitor::meta_count_ = 0;

std::string MonitorHeapMetaToJson(const std::shared_ptr<std::vector<base::MonitorHeapMeta>> &meta) {
  std::stringstream ss;
  ss << R"({"heapMetas":[)";

  std::vector<base::MonitorHeapMeta> &data = *meta;
  for (int i = 0; i < data.size(); ++i) {
    auto &item = data[i];
    ss << R"({"type":")" << item.data.type << R"(","file":")" << item.data.file << R"(","line":)"
       << item.data.line << R"(,"size":)" << item.data.size << R"(,"address":")" << std::hex
       << item.address << std::dec << R"("})";

    if (i != data.size() - 1) {
      ss << ',';
    }
  }

  ss << "]}";
  return ss.str();
}

void MemoryMonitor::Watch(HeapMeta *meta) {
  std::lock_guard<std::mutex> lock(mutex_);
  assert(meta->next == nullptr && meta->prev == nullptr);
  tail_->next = meta;
  meta->prev = tail_;
  tail_ = meta;
  ++meta_count_;
}

void MemoryMonitor::UnWatch(HeapMeta *meta) {
  std::lock_guard<std::mutex> lock(mutex_);
  assert(meta->prev != nullptr || meta->next != nullptr);
  assert(meta_count_ > 0);
  meta->prev->next = meta->next;
  if (tail_ == meta) {
    tail_ = meta->prev;
  } else {
    meta->next->prev = meta->prev;
  }
  --meta_count_;
}

std::shared_ptr<std::vector<MonitorHeapMeta>> MemoryMonitor::CollectAllHeapMeta() {
  std::lock_guard<std::mutex> lock(mutex_);
  /**
   * @attention     此处不可使用TDF_MAKE_SHARED，会死锁
   */
  auto data = std::make_shared<std::vector<MonitorHeapMeta>>(meta_count_);
  auto curr_meta = dummy_head_.next;
  for (int i = 0; i < meta_count_ && curr_meta != nullptr; ++i) {
    memcpy(&(*data)[i].data, &curr_meta->data, sizeof(HeapMetaData));
    (*data)[i].address = reinterpret_cast<char *>(curr_meta) + kHeapMetaSize;
    curr_meta = curr_meta->next;
  }
  return data;
}
}  // namespace base
}  // namespace tdf
