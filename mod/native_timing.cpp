// Passive diagnostics for the verified baseline; see research/timing-contracts.md.
#include <chrono>
#include <cfenv>
#include <cstdio>
#include <mutex>
#include <unordered_map>

#include <rex/chrono/clock.h>
#include <rex/ppc/context.h>

extern "C" void __imp__sub_823AFA68(PPCContext& ctx, uint8_t* base);
extern "C" void __imp__sub_82190990(PPCContext& ctx, uint8_t* base);
extern "C" void __imp__sub_821908D0(PPCContext& ctx, uint8_t* base);
extern "C" void __imp__sub_8212E0C8(PPCContext& ctx, uint8_t* base);
extern "C" void __imp__sub_8249D668(PPCContext& ctx, uint8_t* base);
extern "C" void __imp__sub_821DCD78(PPCContext& ctx, uint8_t* base);
extern "C" void __imp__sub_822D0F50(PPCContext& ctx, uint8_t* base);

namespace {

using SteadyClock = std::chrono::steady_clock;
const auto origin = SteadyClock::now();
std::mutex timing_mutex;
std::unordered_map<uint32_t, SteadyClock::time_point> active_players;
std::unordered_map<uint32_t, SteadyClock::time_point> last_deadline_report;

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

double SinceStart(SteadyClock::time_point now) {
  return std::chrono::duration<double>(now - origin).count();
}

struct UpdateWindow {
  SteadyClock::time_point start;
  uint64_t guest_start;
  uint64_t count = 0;
  double seconds = 0;
};

void RecordUpdate(double seconds) {
  PreserveFloatEnvironment preserve;
  std::lock_guard<std::mutex> lock(timing_mutex);
  const auto now = SteadyClock::now();
  const auto guest_now = rex::chrono::Clock::QueryGuestTickCount();
  const auto frequency = rex::chrono::Clock::guest_tick_frequency();
  static UpdateWindow window{now, guest_now};
  // The first call establishes the interval; its delta precedes that interval.
  static bool initialized = false;
  if (!initialized) {
    initialized = true;
    return;
  }
  ++window.count;
  window.seconds += seconds;
  const double elapsed = std::chrono::duration<double>(now - window.start).count();
  if (elapsed >= 1.0) {
    const double guest_seconds = frequency ? double(guest_now - window.guest_start) / frequency : 0;
    std::fprintf(
        stderr,
        "BRAWLERS_TIMING:update host=%.6f steady_ns=%lld elapsed=%.6f guest=%.6f delta_sum=%.6f "
        "calls=%llu\n",
        SinceStart(now),
        static_cast<long long>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count()),
        elapsed, guest_seconds, window.seconds, static_cast<unsigned long long>(window.count));
    window = {now, guest_now};
  }
}

}  // namespace

extern "C" void sub_823AFA68(PPCContext& ctx, uint8_t* base) {
  {
    PreserveFloatEnvironment preserve;
    const auto now = SteadyClock::now();
    std::fprintf(stderr, "BRAWLERS_TIMING:movie_request host=%.6f id=%u\n", SinceStart(now),
                 ctx.r4.u32);
  }
  __imp__sub_823AFA68(ctx, base);
}

extern "C" void sub_82190990(PPCContext& ctx, uint8_t* base) {
  const uint32_t player = ctx.r3.u32;
  __imp__sub_82190990(ctx, base);
  PreserveFloatEnvironment preserve;
  const auto now = SteadyClock::now();
  const bool succeeded = ctx.r3.u32 != 0;
  if (succeeded) {
    std::lock_guard<std::mutex> lock(timing_mutex);
    active_players.insert_or_assign(player, now);
  }
  std::fprintf(stderr, "BRAWLERS_TIMING:movie_start host=%.6f player=%08x success=%u\n",
               SinceStart(now), player, unsigned(succeeded));
}

extern "C" void sub_821908D0(PPCContext& ctx, uint8_t* base) {
  const uint32_t player = ctx.r3.u32;
  __imp__sub_821908D0(ctx, base);
  PreserveFloatEnvironment preserve;
  const auto now = SteadyClock::now();
  std::lock_guard<std::mutex> lock(timing_mutex);
  const auto found = active_players.find(player);
  if (found != active_players.end()) {
    // The stop routine can return early; this does not establish release or EOF.
    const double elapsed = std::chrono::duration<double>(now - found->second).count();
    std::fprintf(stderr, "BRAWLERS_TIMING:movie_stop_return host=%.6f player=%08x elapsed=%.6f\n",
                 SinceStart(now), player, elapsed);
    active_players.erase(found);
  }
}

extern "C" void sub_8212E0C8(PPCContext& ctx, uint8_t* base) {
  RecordUpdate(ctx.f1.f64);
  __imp__sub_8212E0C8(ctx, base);
}

extern "C" void sub_8249D668(PPCContext& ctx, uint8_t* base) {
  const uint32_t clock = ctx.r3.u32;
  const int32_t requested = ctx.r4.s32;
  const uint32_t caller = static_cast<uint32_t>(ctx.lr);
  __imp__sub_8249D668(ctx, base);
  if (ctx.r3.u32 != 0)
    return;
  PreserveFloatEnvironment preserve;
  const auto now = SteadyClock::now();
  std::lock_guard<std::mutex> lock(timing_mutex);
  const auto found = last_deadline_report.find(clock);
  if (found == last_deadline_report.end() || now - found->second >= std::chrono::seconds(1)) {
    // These clobbered registers are verified in this baseline's implementation.
    // They describe scheduling, not physical display or audio output positions.
    std::fprintf(
        stderr,
        "BRAWLERS_TIMING:media_deadline host=%.6f steady_ns=%lld clock=%08x caller=%08x "
        "requested_ms=%d media_ms=%lld deadline_ms=%d\n",
        SinceStart(now),
        static_cast<long long>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count()),
        clock, caller, requested, static_cast<long long>(ctx.r7.s64), ctx.r5.s32);
    last_deadline_report.insert_or_assign(clock, now);
  }
}

extern "C" void sub_821DCD78(PPCContext& ctx, uint8_t* base) {
  {
    PreserveFloatEnvironment preserve;
    const auto now = SteadyClock::now();
    // This is a game request, not a measurement of audible output.
    std::fprintf(stderr, "BRAWLERS_TIMING:sound_request host=%.6f id=%u argument=%u caller=%08x\n",
                 SinceStart(now), ctx.r3.u32, ctx.r4.u32, static_cast<uint32_t>(ctx.lr));
  }
  __imp__sub_821DCD78(ctx, base);
}

extern "C" void sub_822D0F50(PPCContext& ctx, uint8_t* base) {
  {
    PreserveFloatEnvironment preserve;
    std::lock_guard<std::mutex> lock(timing_mutex);
    const auto now = SteadyClock::now();
    static auto start = now;
    static uint64_t calls = 0;
    ++calls;
    const double elapsed = std::chrono::duration<double>(now - start).count();
    if (elapsed >= 1.0) {
      // Aggregate entry cadence; no screen lifetime or widget state is inferred.
      std::fprintf(stderr,
                   "BRAWLERS_TIMING:menu_transition_update host=%.6f elapsed=%.6f calls=%llu\n",
                   SinceStart(now), elapsed, static_cast<unsigned long long>(calls));
      start = now;
      calls = 0;
    }
  }
  __imp__sub_822D0F50(ctx, base);
}
