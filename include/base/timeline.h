// Copyright (c) 2020 Tencent Corporation. All rights reserved.

#pragma once
#include <pthread.h>
#include <string>
#include <list>
#include <mutex>
#include <thread>
#include <functional>
#include "base/task_runner.h"

namespace tdf {
namespace base {
class TimelineEvent;
class TimelineEventRecorder;
class TimelineEventBlock;

class Timeline {
 public:
  static void Init();
  static std::shared_ptr<TimelineEventRecorder> Recorder();
  static std::vector<std::shared_ptr<TimelineEventRecorder>> CollectRecorders();

  /// ReclaimCachedBlocksFromThreads/CommitBlocks/TimelineEventToJson
  /// must called in the same runner!!!
  static void ReclaimCachedBlocksFromThreads(
      std::vector<std::shared_ptr<TimelineEventRecorder>> &recorders);
  static void CommitBlocks(std::vector<std::shared_ptr<TimelineEventRecorder>> &recorders);
  static std::string TimelineEventToJson();
  static void Clear();

 private:
  static thread_local std::shared_ptr<TimelineEventRecorder> recorder_;

  static std::mutex mutex_;
  static std::vector<std::shared_ptr<TimelineEventRecorder>> recorders_;

  static char *meta_;
  static std::list<std::string> cached_json_;
  static size_t cached_size_;

  static constexpr size_t kMaxCachedSize = 2 * 1024 * 1024;
};

class TimelineEvent {
 public:
  enum class EventType {
    kNone,
    kBegin,
    kEnd,
    kInstant
  };

  TimelineEvent();

  ~TimelineEvent();

  void Instant(std::string label, int64_t micros);

  void Begin(std::string label, int64_t micros);

  void End(std::string label, int64_t micros);

  void Complete();

  bool Within(int64_t time_origin_micros, int64_t time_extent_micros);

  EventType event_type() const { return event_type_; }

  int64_t TimeOrigin() const { return timestamp0_; }

 private:
  void set_timestamp0(int64_t value) { timestamp0_ = value; }

  int64_t timestamp0_;
  pthread_t thread_id_;
  std::string label_;
  EventType event_type_;

  friend class TimelineEventBlock;
  friend class TimelineEventRecorder;

  friend std::ostream &operator<<(std::ostream &out, TimelineEvent &ev);
};

class TimelineEventBlock {
 public:
  explicit TimelineEventBlock(size_t size = 512);
  TimelineEvent *StartEvent();
  bool IsFull() { return w_cursor_ == block_size_; }
  void Finish();

 private:
  void Reset();
  size_t block_size_;
  pthread_t thread_id_;
  std::atomic<size_t> w_cursor_ = 0;
  size_t r_cursor_ = 0;
  std::vector<TimelineEvent> events_;
  std::atomic<TimelineEventBlock *> next_;

  friend class Timeline;
  friend class TimelineEventRecorder;

  friend std::ostream &operator<<(std::ostream &out, TimelineEventBlock &ev);
};

class TimelineEventRecorder {
 public:
  explicit TimelineEventRecorder(size_t max_blocks = 64);

  virtual ~TimelineEventRecorder();

  TimelineEvent *ThreadBlockStartEvent();

  void SetLabel(std::string label);
  const std::string &GetLabel() { return label_; }

  void CompleteEvent(TimelineEvent *event);

  void Clear();

  void RecycleBlock(TimelineEventBlock *blk);

 private:
  TimelineEventBlock *GetNewBlock();

  pthread_t thread_id_;
  size_t max_blocks_count_;
  TimelineEventBlock *current_blk_;
  TimelineEventBlock dummy_blk_;
  TimelineEventBlock *head_blk_, *tail_blk_;
  std::vector<TimelineEventBlock *> reclaimed_blks;
  TimelineEventBlock *blk_pool_;
  std::vector<TimelineEventBlock *> free_blks;
  std::string label_;

  friend class Timeline;
};
}  // namespace base
}  // namespace tdf
