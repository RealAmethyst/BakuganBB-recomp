#pragma once

#include <array>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string_view>
#include <thread>
#include <vector>

#include "native_speech.h"

namespace brawlers::ui {

// A bounded, single-use queue. Construct and Start before guest execution.
// Sinks run on its worker and must not acquire guest/runtime locks, call back
// into this queue, or retain event references. Their lifetime covers Stop.
class GameSpeechQueue {
 public:
  using EventSink = std::function<bool(const ScreenEvent&)>;
  using DiagnosticSink = std::function<bool(std::string_view)>;

  enum class Failure {
    None, StartFailed, CaptureFailed, CapacityExceeded, InvalidDiagnostic,
    OutputFailed, OutputException
  };
  struct Stats {
    uint64_t accepted = 0;
    uint64_t completed = 0;
    uint64_t discarded = 0;
    uint64_t rejected = 0;
    uint64_t output_failures = 0;
    Failure failure = Failure::None;
  };

  // Ring allocation and sink construction can throw; do this before guests start.
  GameSpeechQueue(EventSink events, DiagnosticSink diagnostics, size_t capacity = 256);
  ~GameSpeechQueue();
  GameSpeechQueue(const GameSpeechQueue&) = delete;
  GameSpeechQueue& operator=(const GameSpeechQueue&) = delete;

  // The owner serializes Start/Stop/destruction outside guest/global locks.
  // No restart is supported, including after an output fault.
  bool Start() noexcept;
  // Drains and joins. A blocked sink can delay Flush/Stop; sinks must return.
  bool Stop();

  // Never waits for space or invokes sinks. Event preparation may allocate at
  // the caller, but moving an event into the preallocated ring does not.
  // Overflow latches a fault: drain the accepted prefix, report once, reject
  // subsequent entries. Return false must not be treated as successful delivery.
  // Guest callers hold the runtime global region before these queue operations,
  // including FailCapture; the queue itself has no runtime dependency.
  bool Enqueue(ScreenEvent&& event) noexcept;
  // Copies a nonempty ASCII [a-z0-9_] reason key, at most 95 bytes. No text or pointers.
  bool EnqueueDiagnostic(std::string_view reason) noexcept;
  // Call when preparing an event fails before Enqueue (for example allocation).
  // A missing lifecycle event must prevent later events from being delivered.
  void FailCapture() noexcept;

  // Waits for entries accepted before this call and any fault report. Never
  // call while holding guest/global locks or from a sink. It fences output
  // attempts, not screen-reader playback. False indicates a stopped/faulted queue.
  bool Flush();
  Stats stats() const;

 private:
  struct Entry {
    ScreenEvent event;
    std::array<char, 96> reason{};
    size_t reason_length = 0;
  };
  bool CanEnqueueLocked();
  void FailLocked(Failure failure);
  void Run() noexcept;
  static std::string_view FailureReason(Failure failure);

  EventSink events_;
  DiagnosticSink diagnostics_;
  mutable std::mutex mutex_;
  std::condition_variable ready_;
  std::condition_variable completed_;
  std::vector<std::optional<Entry>> entries_;
  size_t head_ = 0;
  size_t tail_ = 0;
  size_t pending_ = 0;
  Stats stats_;
  bool started_ = false;
  bool running_ = false;
  bool stopping_ = false;
  bool fault_reported_ = false;
  bool discard_pending_ = false;
  std::thread worker_;
};

}  // namespace brawlers::ui
