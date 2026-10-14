#include <rex/thread.h>
#include <chrono>
#include <cstdio>
#include <thread>
using namespace std::chrono_literals;
using R = rex::thread::WaitResult;
int failures = 0, checks = 0;
void check(const char* label, bool passed) {
  checks++;
  failures += !passed;
  std::printf("%s %s\n", passed ? "PASS" : "FAIL", label);
}
int main() {
  auto a = rex::thread::Event::CreateAutoResetEvent(true);
  check("single_zero_ready", rex::thread::Wait(a.get(), true, 0ms) == R::kSuccess);
  check("single_consumed_once", rex::thread::Wait(a.get(), false, 0ms) == R::kTimeout);
  check("zero_unsignaled", rex::thread::Wait(a.get(), true, 0ms) == R::kTimeout);
  a->Set();
  check("nonalertable_control", rex::thread::Wait(a.get(), false, 0ms) == R::kSuccess);
  auto b = rex::thread::Event::CreateAutoResetEvent(true);
  a->Set();
  rex::thread::WaitHandle* pair[] = {a.get(), b.get()};
  auto any = rex::thread::WaitAny(pair, 2, true, 0ms);
  check("any_first_ready", any.first == R::kSuccess && any.second == 0);
  check("any_consumes_chosen", rex::thread::Wait(a.get(), false, 0ms) == R::kTimeout);
  check("any_preserves_other", rex::thread::Wait(b.get(), false, 0ms) == R::kSuccess);
  a->Set();
  b->Reset();
  check("all_partial_times_out", rex::thread::WaitAll(pair, 2, true, 0ms) == R::kTimeout);
  check("all_partial_preserves_ready", rex::thread::Wait(a.get(), false, 0ms) == R::kSuccess);
  a->Set();
  b->Set();
  check("all_ready", rex::thread::WaitAll(pair, 2, true, 0ms) == R::kSuccess);
  check("all_consumes_first", rex::thread::Wait(a.get(), false, 0ms) == R::kTimeout);
  check("all_consumes_second", rex::thread::Wait(b.get(), false, 0ms) == R::kTimeout);
  a->Reset();
  b->Set();
  check("signal_wait_ready",
        rex::thread::SignalAndWait(a.get(), b.get(), true, 0ms) == R::kSuccess);
  check("signal_side_effect", rex::thread::Wait(a.get(), false, 0ms) == R::kSuccess);
  check("signal_wait_consumes_target", rex::thread::Wait(b.get(), false, 0ms) == R::kTimeout);
  int callback_order = 0;
  auto* self = rex::thread::Thread::GetCurrentThread();
  a->Set();
  self->QueueUserCallback([&] { callback_order = callback_order * 10 + 1; });
  self->QueueUserCallback([&] { callback_order = callback_order * 10 + 2; });
  check("queued_apc_precedes_ready_single",
        rex::thread::Wait(a.get(), true, 0ms) == R::kUserCallback);
  check("second_queued_apc_precedes_ready",
        rex::thread::Wait(a.get(), true, 0ms) == R::kUserCallback);
  check("queued_apc_fifo", callback_order == 12);
  check("apc_does_not_consume_single", rex::thread::Wait(a.get(), false, 0ms) == R::kSuccess);
  for (bool all : {false, true}) {
    a->Set();
    b->Set();
    self->QueueUserCallback([&] { callback_order++; });
    auto value = all ? rex::thread::WaitAll(pair, 2, true, 0ms)
                     : rex::thread::WaitAny(pair, 2, true, 0ms).first;
    check(all ? "queued_apc_precedes_all" : "queued_apc_precedes_any", value == R::kUserCallback);
    check(all ? "apc_preserves_all_first" : "apc_preserves_any_first",
          rex::thread::Wait(a.get(), false, 0ms) == R::kSuccess);
    check(all ? "apc_preserves_all_second" : "apc_preserves_any_second",
          rex::thread::Wait(b.get(), false, 0ms) == R::kSuccess);
  }
  a->Reset();
  b->Set();
  self->QueueUserCallback([&] { callback_order++; });
  check("queued_apc_precedes_signal_wait",
        rex::thread::SignalAndWait(a.get(), b.get(), true, 0ms) == R::kUserCallback);
  check("apc_signal_occurs", rex::thread::Wait(a.get(), false, 0ms) == R::kSuccess);
  check("apc_preserves_target", rex::thread::Wait(b.get(), false, 0ms) == R::kSuccess);
  a->Set();
  check("infinite_ready", rex::thread::Wait(a.get(), true) == R::kSuccess);
  std::thread signaler([&] {
    std::this_thread::sleep_for(5ms);
    a->Set();
  });
  check("finite_event_arrival", rex::thread::Wait(a.get(), true, 100ms) == R::kSuccess);
  signaler.join();
  check("finite_unsignaled_timeout", rex::thread::Wait(a.get(), true, 5ms) == R::kTimeout);
  std::printf("SUMMARY checks=%d failures=%d\n", checks, failures);
  return failures ? 1 : 0;
}
