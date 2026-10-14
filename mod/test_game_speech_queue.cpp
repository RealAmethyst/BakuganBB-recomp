#include "game_speech_queue.h"

#include <atomic>
#include <cassert>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using brawlers::ui::GameSpeechQueue;

namespace {

ScreenEvent Event(uint64_t id) {
  ScreenEvent event;
  event.id = id;
  event.label = "Synthetic caption " + std::to_string(id);
  return event;
}

class Gate {
 public:
  void Enter() {
    std::unique_lock<std::mutex> lock(mutex_);
    entered_ = true;
    changed_.notify_all();
    changed_.wait(lock, [&] { return open_; });
  }
  void Wait() {
    std::unique_lock<std::mutex> lock(mutex_);
    changed_.wait(lock, [&] { return entered_; });
  }
  void Open() {
    std::lock_guard<std::mutex> lock(mutex_);
    open_ = true;
    changed_.notify_all();
  }

 private:
  std::mutex mutex_;
  std::condition_variable changed_;
  bool entered_ = false;
  bool open_ = false;
};

void OrderedValues() {
  std::vector<std::string> output;
  const auto producer = std::this_thread::get_id();
  GameSpeechQueue queue([&](const ScreenEvent& event) {
    assert(std::this_thread::get_id() != producer);
    output.push_back(event.label);
    return true;
  }, [&](std::string_view reason) {
    assert(std::this_thread::get_id() != producer);
    output.emplace_back(reason);
    return true;
  }, 8);
  assert(!queue.Flush());
  assert(!queue.Enqueue(Event(0)));
  assert(queue.Start());
  assert(!queue.Start());
  auto event = Event(1);
  assert(queue.Enqueue(std::move(event)));
  event.label = "Changed after move";
  std::string reason = "synthetic_guard";
  assert(queue.EnqueueDiagnostic(reason));
  reason.assign("changed_guard");
  assert(queue.Enqueue(Event(2)));
  assert(queue.Flush());
  assert((output == std::vector<std::string>{"Synthetic caption 1", "synthetic_guard",
                                           "Synthetic caption 2"}));
  assert(queue.stats().accepted == 3);
  assert(queue.stats().completed == 3);
  assert(queue.Stop());
  assert(queue.Stop());
  assert(!queue.Start());
  assert(!queue.Enqueue(Event(3)));
}

void Overflow() {
  Gate gate;
  std::vector<uint64_t> output;
  std::vector<std::string> diagnostics;
  GameSpeechQueue queue([&](const ScreenEvent& event) {
    if (event.id == 1)
      gate.Enter();
    output.push_back(event.id);
    return true;
  }, [&](std::string_view reason) {
    diagnostics.emplace_back(reason);
    return true;
  }, 2);
  assert(queue.Start());
  assert(queue.Enqueue(Event(1)));
  gate.Wait();
  // The worker has removed item 1 and released the queue mutex before its sink.
  assert(queue.Enqueue(Event(2)));
  assert(queue.Enqueue(Event(3)));
  assert(!queue.Enqueue(Event(4)));
  assert(!queue.Enqueue(Event(5)));
  gate.Open();
  assert(!queue.Flush());
  assert((output == std::vector<uint64_t>{1, 2, 3}));
  assert((diagnostics == std::vector<std::string>{"queue_capacity_exceeded"}));
  const auto stats = queue.stats();
  assert(stats.accepted == 3 && stats.completed == 3 && stats.discarded == 0);
  assert(stats.rejected == 2 && stats.failure == GameSpeechQueue::Failure::CapacityExceeded);
  assert(!queue.Stop());
  assert(!queue.Start());
}

void OutputFailure(bool throws, bool diagnostic_throws) {
  Gate gate;
  std::atomic<unsigned> calls{0};
  std::vector<std::string> diagnostics;
  GameSpeechQueue queue([&](const ScreenEvent&) {
    ++calls;
    gate.Enter();
    if (throws)
      throw std::runtime_error("synthetic sink failure");
    return false;
  }, [&](std::string_view reason) {
    diagnostics.emplace_back(reason);
    if (diagnostic_throws)
      throw std::runtime_error("synthetic diagnostic failure");
    return !throws;  // Exercise false as well as throwing failure diagnostics.
  }, 4);
  assert(queue.Start());
  assert(queue.Enqueue(Event(1)));
  gate.Wait();
  assert(queue.Enqueue(Event(2)));
  assert(queue.EnqueueDiagnostic("synthetic_guard"));
  gate.Open();
  assert(!queue.Flush());
  assert(calls == 1);
  assert(diagnostics.size() == 1);
  assert(diagnostics.front() == (throws ? "queue_output_exception" : "queue_output_failed"));
  const auto stats = queue.stats();
  assert(stats.completed == 3 && stats.discarded == 2);
  assert(stats.output_failures == ((throws || diagnostic_throws) ? 2 : 1));
  assert(!queue.Enqueue(Event(3)));
  assert(!queue.Stop());
}

void CaptureAndDiagnosticFailures() {
  std::vector<std::string> diagnostics;
  {
    GameSpeechQueue queue([](const ScreenEvent&) { return true; },
        [&](std::string_view reason) { diagnostics.emplace_back(reason); return true; });
    assert(queue.Start());
    queue.FailCapture();
    assert(!queue.Enqueue(Event(1)));
    assert(!queue.Flush());
    assert(diagnostics.back() == "queue_capture_failed");
  }
  for (const std::string& bad : {std::string{}, std::string("line\nbreak"),
                                std::string(96, 'a'), std::string("Visible caption")}) {
    GameSpeechQueue queue([](const ScreenEvent&) { return true; },
        [&](std::string_view reason) { diagnostics.emplace_back(reason); return true; });
    assert(queue.Start());
    assert(!queue.EnqueueDiagnostic(bad));
    assert(!queue.Flush());
    assert(diagnostics.back() == "queue_invalid_diagnostic");
  }
  {
    GameSpeechQueue queue([](const ScreenEvent&) { assert(false); return true; },
        [](std::string_view) { return false; });
    assert(queue.Start());
    assert(queue.EnqueueDiagnostic("synthetic_guard"));
    assert(!queue.Flush());
    assert(queue.stats().output_failures == 2);
  }
}

void ConcurrentProducersAndDrain() {
  std::vector<uint64_t> output;
  {
    GameSpeechQueue queue([&](const ScreenEvent& event) {
      output.push_back(event.id);
      return true;
    }, [](std::string_view) { assert(false); return false; }, 1024);
    assert(queue.Start());
    std::vector<std::thread> producers;
    for (uint64_t producer = 0; producer < 4; ++producer) {
      producers.emplace_back([&, producer] {
        for (uint64_t n = 0; n < 200; ++n)
          assert(queue.Enqueue(Event(producer * 1000 + n)));
      });
    }
    for (auto& producer : producers)
      producer.join();
    // Destruction must drain accepted entries and join before captures expire.
  }
  assert(output.size() == 800);
  std::array<uint64_t, 4> next{};
  for (auto id : output) {
    const auto producer = id / 1000;
    assert(producer < next.size());
    assert(id % 1000 == next[producer]++);
  }
  for (auto count : next)
    assert(count == 200);
}

void InvalidSetup() {
  bool caught = false;
  try {
    GameSpeechQueue queue([](const ScreenEvent&) { return true; },
                          [](std::string_view) { return true; }, 0);
  } catch (const std::invalid_argument&) { caught = true; }
  assert(caught);
  GameSpeechQueue queue([](const ScreenEvent&) { return true; },
                        [](std::string_view) { return true; });
  assert(queue.Stop());
  assert(!queue.Start());
}

}  // namespace

int main() {
  OrderedValues();
  Overflow();
  OutputFailure(false, false);
  OutputFailure(true, false);
  OutputFailure(false, true);
  CaptureAndDiagnosticFailures();
  ConcurrentProducersAndDrain();
  InvalidSetup();
}
