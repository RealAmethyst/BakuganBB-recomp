// Passive keyboard-request diagnostics; see research/story-entry-freeze.md.
#include <atomic>
#include <cfenv>
#include <chrono>
#include <cstdio>
#include <mutex>

#include <rex/ppc/context.h>

extern "C" void __imp__sub_823BC4F0(PPCContext& ctx, uint8_t* base);
extern "C" void __imp__sub_823B6A40(PPCContext& ctx, uint8_t* base);

namespace {

using SteadyClock = std::chrono::steady_clock;
std::atomic<unsigned long long> next_request{0};
std::mutex poll_mutex;

class PreserveFloatEnvironment {
 public:
  PreserveFloatEnvironment() : valid_(std::fegetenv(&environment_) == 0) {}
  ~PreserveFloatEnvironment() {
    if (valid_)
      std::fesetenv(&environment_);
  }

 private:
  std::fenv_t environment_;
  bool valid_;
};

long long Timestamp(SteadyClock::time_point now) {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
}

}  // namespace

extern "C" void sub_823BC4F0(PPCContext& ctx, uint8_t* base) {
  const auto request = ++next_request;
  {
    PreserveFloatEnvironment preserve;
    // This leaf tail-calls XamShowKeyboardUI with these registers unchanged.
    // Never dereference or record the default text, caption or edited name.
    std::fprintf(stderr,
                 "BRAWLERS_DIALOG:keyboard_request steady_ns=%lld request=%llu caller=%08x "
                 "user=%u flags=%08x capacity=%u overlap=%08x\n",
                 Timestamp(SteadyClock::now()), request, static_cast<uint32_t>(ctx.lr),
                 ctx.r3.u32, ctx.r4.u32, ctx.r9.u32, ctx.r10.u32);
  }
  __imp__sub_823BC4F0(ctx, base);
  {
    PreserveFloatEnvironment preserve;
    // IO_PENDING is dispatch acceptance, not proof that a dialog was drawn.
    std::fprintf(stderr,
                 "BRAWLERS_DIALOG:keyboard_return steady_ns=%lld request=%llu result=%08x\n",
                 Timestamp(SteadyClock::now()), request, ctx.r3.u32);
  }
}

extern "C" void sub_823B6A40(PPCContext& ctx, uint8_t* base) {
  const uint32_t owner = ctx.r3.u32;
  const uint32_t caller = static_cast<uint32_t>(ctx.lr);
  __imp__sub_823B6A40(ctx, base);
  PreserveFloatEnvironment preserve;
  std::lock_guard<std::mutex> lock(poll_mutex);
  const auto now = SteadyClock::now();
  const uint32_t result = ctx.r3.u32;
  static bool initialized = false;
  static uint32_t last_owner = 0, last_caller = 0, last_result = 0;
  static unsigned long long polls = 0;
  static auto last_report = now;
  const bool changed = !initialized || owner != last_owner || caller != last_caller ||
                       result != last_result;
  ++polls;
  if (changed || now - last_report >= std::chrono::seconds(1)) {
    // Raw native status; status 3 includes any nonzero completion error.
    // polls counts all calls since the preceding report, not one status's lifetime.
    std::fprintf(stderr,
                 "BRAWLERS_DIALOG:keyboard_poll steady_ns=%lld owner=%08x caller=%08x "
                 "result=%u polls=%llu\n",
                 Timestamp(now), owner, caller, result, polls);
    initialized = true;
    last_owner = owner;
    last_caller = caller;
    last_result = result;
    polls = 0;
    last_report = now;
  }
}
