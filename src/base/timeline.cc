// Copyright (c) 2020 Tencent Corporation. All rights reserved.

/**
 * @brief A lock-free tracing module
 */

/**
``` mermaid
classDiagram
    Timeline "1"o--"* (per thread)" TimelineRecorder
    TimelineRecorder "1"*--"*" TimelineEventBlock
    TimelineEventBlock "1"o--"n" TimelineEvent
```
 */

#include "base/timeline.h"
#include <pthread.h>
#include <cmath>
#include <sstream>
#include "base/logging.h"

namespace tdf {
namespace base {
TimelineEvent::TimelineEvent() : timestamp0_(0), event_type_(EventType::kNone) {}

TimelineEvent::~TimelineEvent() { event_type_ = EventType::kNone; }

void TimelineEvent::Instant(std::string label, int64_t micros) {
  event_type_ = EventType::kInstant;
  label_ = move(label);
  set_timestamp0(micros);
}

void TimelineEvent::Begin(std::string label, int64_t micros) {
  event_type_ = EventType::kBegin;
  label_ = move(label);
  set_timestamp0(micros);
}

void TimelineEvent::End(std::string label, int64_t micros) {
  event_type_ = EventType::kEnd;
  label_ = move(label);
  set_timestamp0(micros);
}

void TimelineEvent::Complete() {
  auto recorder = Timeline::Recorder();
  if (recorder != nullptr) {
    recorder->CompleteEvent(this);
  }
}

bool TimelineEvent::Within(int64_t time_origin_micros, int64_t time_extent_micros) {
  if ((time_origin_micros == -1) || (time_extent_micros == -1)) {
    // No time range specified.
    return true;
  }
  int64_t delta = TimeOrigin() - time_origin_micros;
  return (delta >= 0) && (delta <= time_extent_micros);
}

std::ostream &operator<<(std::ostream &out, TimelineEvent &ev) {
  out << R"({"name":")" << ev.label_ << R"(","ts":)" << ev.timestamp0_ << R"(,"pid":0,"tid":)";
#if defined(__APPLE__)
  out << pthread_mach_thread_np(ev.thread_id_);
#else
  out << pthread_gettid_np(ev.thread_id_);
#endif
  switch (ev.event_type()) {
    case TimelineEvent::EventType::kBegin:
      out << R"(,"ph":"B")";
      break;
    case TimelineEvent::EventType::kEnd:
      out << R"(,"ph":"E")";
      break;
    case TimelineEvent::EventType::kInstant:
      out << R"(,"ph":"i","s":"p")";
      break;
    case TimelineEvent::EventType::kNone:
      break;
  }
  out << '}';
  return out;
}

void Timeline::Init() {}

std::mutex Timeline::mutex_;
thread_local std::shared_ptr<TimelineEventRecorder> Timeline::recorder_ = nullptr;
std::shared_ptr<TimelineEventRecorder> Timeline::Recorder() {
  if (recorder_ == nullptr) {
    std::lock_guard<std::mutex> lock(mutex_);
    recorder_ = std::make_shared<TimelineEventRecorder>();
    recorders_.push_back(recorder_);
  }

  return recorder_;
}

std::vector<std::shared_ptr<TimelineEventRecorder>> Timeline::recorders_;
void Timeline::Clear() {}

void Timeline::ReclaimCachedBlocksFromThreads(
    std::vector<std::shared_ptr<TimelineEventRecorder>> &recorders) {
  for (auto recorder : recorders) {
    auto curr = recorder->head_blk_->next_.load();
    while (curr != nullptr) {
      auto next = curr->next_.load();
      recorder->reclaimed_blks.push_back(curr);

      if (next != nullptr) {
        recorder->head_blk_->next_.store(next);
      }
      curr = next;
    }
  }
}

char *Timeline::meta_;
std::list<std::string> Timeline::cached_json_;
size_t Timeline::cached_size_ = 0;
void Timeline::CommitBlocks(std::vector<std::shared_ptr<TimelineEventRecorder>> &recorders) {
  std::stringstream ss;

  ///  std::vector<std::vector<TimelineEventBlock *>>
  for (auto &foo : recorders) {
    ///  std::vector<TimelineEventBlock *>
    auto count = foo->reclaimed_blks.size();
    for (auto i = 0; i < count; ++i) {
      auto blk = foo->reclaimed_blks[i];
      if (blk->r_cursor_ != blk->block_size_) {
        ss << *blk;
      }

      if (i < count - 1) {
        foo->RecycleBlock(blk);
      }
    }
    foo->reclaimed_blks.clear();
  }

  auto json = ss.str();
  if (json.empty()) {
    return;
  }

  cached_size_ += json.length();
  cached_json_.push_back(std::move(json));

  while (cached_size_ > kMaxCachedSize) {
    cached_size_ -= cached_json_.begin()->length();
    cached_json_.pop_front();
  }

  std::stringstream meta_ss;
  for (int64_t i = 0; i < recorders.size(); ++i) {
    auto &foo = recorders[i];
    meta_ss << R"({"cat":"__metadata","pid":0,"tid":)";
#if defined(__APPLE__)
    meta_ss << pthread_mach_thread_np(foo->thread_id_);
#else
    meta_ss << pthread_gettid_np(foo->thread_id_);
#endif
    meta_ss << R"(,"ts":0,"ph":"M","name":"thread_name","args":{"name":")" << foo->GetLabel()
            << R"("}})";
    if (i < recorders.size() - 1) {
      meta_ss << ',';
    }
  }

  std::string meta_str = meta_ss.str();
  const char* meta_cstr = meta_str.c_str();
  if (meta_ == nullptr || strcmp(meta_cstr, meta_) != 0) {
     meta_ = strdup(meta_cstr);
  }
}

std::string Timeline::TimelineEventToJson() {
  if (cached_json_.empty()) {
    return R"({"traceEvents":[]})";
  }

  std::stringstream ss;
  ss << R"({"traceEvents":[)";

  for (auto &json : cached_json_) {
    ss << json;
  }
  cached_json_.clear();
  cached_size_ = 0;

  ss << meta_ << "]}";
  meta_ = nullptr;

  return ss.str();
}

std::vector<std::shared_ptr<TimelineEventRecorder>> Timeline::CollectRecorders() {
  std::lock_guard<std::mutex> lock(mutex_);
  return std::vector<std::shared_ptr<TimelineEventRecorder>>(recorders_);
}

TimelineEventBlock::TimelineEventBlock(size_t size) : block_size_(size), events_(size) {
  next_.store(nullptr);
  w_cursor_.store(0);
}

TimelineEvent *TimelineEventBlock::StartEvent() {
  assert(!IsFull());
  auto ev = &events_[w_cursor_++];
  ev->thread_id_ = thread_id_;
  return ev;
}

void TimelineEventBlock::Finish() {
  //  Timeline::Recorder()->FinishBlock(this);
}

std::ostream &operator<<(std::ostream &out, TimelineEventBlock &blk) {
  for (auto len = blk.w_cursor_.load(); blk.r_cursor_ < len; ++blk.r_cursor_) {
    out << blk.events_[blk.r_cursor_] << ',';
  }

  return out;
}
void TimelineEventBlock::Reset() {
  r_cursor_ = 0;
  w_cursor_.store(0);
  next_.store(nullptr);
}

TimelineEventBlock *TimelineEventRecorder::GetNewBlock() {
  if (free_blks.empty()) {
    return nullptr;
  }

  auto blk = free_blks.back();
  free_blks.pop_back();

  blk->thread_id_ = thread_id_;
  tail_blk_->next_.store(blk);
  tail_blk_ = blk;

  return blk;
}

TimelineEvent *TimelineEventRecorder::ThreadBlockStartEvent() {
  if (current_blk_ == nullptr) {
    current_blk_ = GetNewBlock();
  } else if (current_blk_->IsFull()) {
    current_blk_->Finish();
    current_blk_ = GetNewBlock();
  }

  return current_blk_ != nullptr ? current_blk_->StartEvent() : nullptr;
}

void TimelineEventRecorder::CompleteEvent(TimelineEvent *event) {
  //  FinishThreadBlockIfNeeded();
}

void TimelineEventRecorder::Clear() {
  current_blk_ = nullptr;
  head_blk_ = tail_blk_ = &dummy_blk_;
  delete []blk_pool_;
}

TimelineEventRecorder::TimelineEventRecorder(size_t max_blocks)
    : max_blocks_count_(max_blocks),
      current_blk_(nullptr),
      dummy_blk_(0),
      head_blk_(&dummy_blk_),
      tail_blk_(&dummy_blk_),
      free_blks(max_blocks_count_) {
  thread_id_ = pthread_self();
  dummy_blk_.next_.store(nullptr);
  dummy_blk_.w_cursor_.store(0);
  dummy_blk_.r_cursor_ = 0;

  blk_pool_ = new TimelineEventBlock[max_blocks_count_];
  for (int64_t i = 0; i < max_blocks_count_; ++i) {
    free_blks[i] = &blk_pool_[i];
  }

  std::stringstream ss;
  ss << "thread#0x" << std::hex << thread_id_ << std::dec;
  label_ = ss.str();

  TDF_BASE_LOG(INFO) << "new TimelineEventRecorder for thread#" << std::hex << thread_id_
                     << std::dec << " and alloc " << max_blocks_count_ << " blocks";
}

TimelineEventRecorder::~TimelineEventRecorder() {
  TDF_BASE_LOG(INFO) << "destroy TimelineEventRecorder of thread#" << std::hex << thread_id_
                     << std::dec;
  Clear();
}

void TimelineEventRecorder::RecycleBlock(TimelineEventBlock *blk) {
  blk->Reset();
  free_blks.push_back(blk);
}
void TimelineEventRecorder::SetLabel(std::string label) { label_ = move(label); }

}  // namespace base
}  // namespace tdf
