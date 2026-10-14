#include "game_speech_queue.h"

#include <algorithm>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace brawlers::ui {

GameSpeechQueue::GameSpeechQueue(EventSink events, DiagnosticSink diagnostics, size_t capacity)
    : events_(std::move(events)), diagnostics_(std::move(diagnostics)), entries_(capacity) {
  if (!capacity || !events_ || !diagnostics_)
    throw std::invalid_argument("speech queue requires capacity and sinks");
  static_assert(std::is_nothrow_move_constructible_v<ScreenEvent>);
}

GameSpeechQueue::~GameSpeechQueue() { Stop(); }

bool GameSpeechQueue::Start() noexcept {
  std::lock_guard<std::mutex> lock(mutex_);
  if (started_ || stopping_ || stats_.failure != Failure::None)
    return false;
  started_ = true;
  try {
    worker_ = std::thread([this] { Run(); });
    running_ = true;
    return true;
  } catch (...) {
    stats_.failure = Failure::StartFailed;
    return false;
  }
}

bool GameSpeechQueue::Stop() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stopping_ = true;
    ready_.notify_one();
  }
  if (worker_.joinable())
    worker_.join();
  std::lock_guard<std::mutex> lock(mutex_);
  return stats_.failure == Failure::None;
}

void GameSpeechQueue::FailLocked(Failure failure) {
  if (stats_.failure == Failure::None)
    stats_.failure = failure;
  ready_.notify_one();
}

void GameSpeechQueue::FailCapture() noexcept {
  std::lock_guard<std::mutex> lock(mutex_);
  FailLocked(Failure::CaptureFailed);
}

bool GameSpeechQueue::CanEnqueueLocked() {
  if (!running_ || stopping_ || stats_.failure != Failure::None) {
    ++stats_.rejected;
    return false;
  }
  if (pending_ == entries_.size()) {
    ++stats_.rejected;
    FailLocked(Failure::CapacityExceeded);
    return false;
  }
  return true;
}

bool GameSpeechQueue::Enqueue(ScreenEvent&& event) noexcept {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!CanEnqueueLocked())
    return false;
  entries_[tail_].emplace(Entry{std::move(event), {}, 0});
  tail_ = (tail_ + 1) % entries_.size();
  ++pending_;
  ++stats_.accepted;
  ready_.notify_one();
  return true;
}

bool GameSpeechQueue::EnqueueDiagnostic(std::string_view reason) noexcept {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!CanEnqueueLocked())
    return false;
  const bool valid = !reason.empty() && reason.size() < Entry{}.reason.size() &&
      std::all_of(reason.begin(), reason.end(), [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
      });
  if (!valid) {
    ++stats_.rejected;
    FailLocked(Failure::InvalidDiagnostic);
    return false;
  }
  auto& entry = entries_[tail_].emplace();
  std::copy(reason.begin(), reason.end(), entry.reason.begin());
  entry.reason_length = reason.size();
  tail_ = (tail_ + 1) % entries_.size();
  ++pending_;
  ++stats_.accepted;
  ready_.notify_one();
  return true;
}

bool GameSpeechQueue::Flush() {
  std::unique_lock<std::mutex> lock(mutex_);
  if (!running_)
    return false;
  const uint64_t target = stats_.accepted;
  completed_.wait(lock, [&] {
    return stats_.completed >= target &&
           (stats_.failure == Failure::None || fault_reported_);
  });
  return stats_.failure == Failure::None;
}

GameSpeechQueue::Stats GameSpeechQueue::stats() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return stats_;
}

std::string_view GameSpeechQueue::FailureReason(Failure failure) {
  switch (failure) {
    case Failure::StartFailed: return "queue_start_failed";
    case Failure::CaptureFailed: return "queue_capture_failed";
    case Failure::CapacityExceeded: return "queue_capacity_exceeded";
    case Failure::InvalidDiagnostic: return "queue_invalid_diagnostic";
    case Failure::OutputException: return "queue_output_exception";
    case Failure::OutputFailed: return "queue_output_failed";
    case Failure::None: return "queue_no_failure";
  }
  return "queue_unknown_failure";
}

void GameSpeechQueue::Run() noexcept {
  for (;;) {
    std::optional<Entry> entry;
    Failure report = Failure::None;
    bool discard = false;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      ready_.wait(lock, [&] {
        return pending_ || stopping_ ||
               (stats_.failure != Failure::None && !fault_reported_);
      });
      if (pending_) {
        entry = std::move(entries_[head_]);
        entries_[head_].reset();
        head_ = (head_ + 1) % entries_.size();
        --pending_;
        discard = discard_pending_;
      } else if (stats_.failure != Failure::None && !fault_reported_) {
        report = stats_.failure;
      } else if (stopping_) {
        running_ = false;
        completed_.notify_all();
        return;
      }
    }

    Failure output_failure = Failure::None;
    try {
      bool success = true;
      if (report != Failure::None)
        success = diagnostics_(FailureReason(report));
      else if (!discard && entry->reason_length)
        success = diagnostics_({entry->reason.data(), entry->reason_length});
      else if (!discard)
        success = events_(entry->event);
      if (!success)
        output_failure = Failure::OutputFailed;
    } catch (...) {
      output_failure = Failure::OutputException;
    }

    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (output_failure != Failure::None) {
        ++stats_.output_failures;
        FailLocked(output_failure);
        // A failed lifecycle event makes subsequent events unsafe to deliver.
        discard_pending_ = true;
      }
      if (report != Failure::None)
        fault_reported_ = true;  // One attempt, even when the diagnostic sink fails.
      else {
        ++stats_.completed;
        if (discard)
          ++stats_.discarded;
      }
      completed_.notify_all();
    }
  }
}

}  // namespace brawlers::ui
