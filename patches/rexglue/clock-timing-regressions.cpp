#include <utility>
#include <cmath>
#include <atomic>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>
#include <string>
#include <limits>
#include <rex/chrono/chrono.h>
using rex::chrono::Clock;
namespace rex::chrono {
extern uint64_t last_guest_tick_count_, last_host_tick_count_;
}
static std::atomic<uint64_t> host_ticks{0};
static std::atomic<bool> intercept{false};
unsigned probe_warning_count = 0;
static bool sampled = false, released = false, second_sampled = false,
            second_sampled_before_release = false;
static std::atomic<bool> track_second{false};
static std::mutex sample_mutex;
static std::condition_variable sample_cv;
uint64_t Clock::host_tick_frequency_platform() {
  return 1000000000;
}
uint64_t Clock::host_tick_count_platform() {
  uint64_t sample = host_ticks.load();
  if (intercept.exchange(false)) {
    std::unique_lock lock(sample_mutex);
    sampled = true;
    sample_cv.notify_all();
    sample_cv.wait(lock, [] { return released; });
  } else if (track_second) {
    std::lock_guard lock(sample_mutex);
    second_sampled = true;
    sample_cv.notify_all();
  }
  return sample;
}
uint64_t Clock::QueryHostSystemTime() {
  return 133000000000000000ull + host_ticks.load() / 100;
}
uint64_t Clock::QueryHostUptimeMillis() {
  return host_ticks.load() / 1000000;
}
void reset(double scale = 1.0) {
  host_ticks = 0;
  clock_no_scaling = false;
  Clock::set_guest_tick_frequency(50000000);
  Clock::set_guest_time_scalar(scale == 2.0 ? 1.0 : 2.0);
  Clock::set_guest_time_scalar(scale);
  Clock::set_guest_system_time_base(Clock::QueryHostSystemTime());
  rex::chrono::last_guest_tick_count_ = rex::chrono::last_host_tick_count_ = 0;
}
template <typename A, typename B>
void report(const char* name, A actual, B expected) {
  std::cout << "{\"name\":\"" << name << "\",\"actual\":" << actual << ",\"expected\":" << expected
            << ",\"equal\":" << (actual == expected ? "true" : "false") << "}\n";
}
int main(int argc, char**) {
  reset();
  host_ticks = 1000000000;
  report("scale1_one_second", Clock::QueryGuestTickCount(), 50000000ull);
  reset();
  for (int i = 0; i < 100000; ++i) {
    host_ticks += 10019;
    Clock::QueryGuestTickCount();
  }
  report("scale1_partition_independence_10019ns", Clock::QueryGuestTickCount(), 50095000ull);
  reset();
  for (int i = 0; i < 50000; ++i) {
    host_ticks += 19;
    Clock::QueryGuestTickCount();
  }
  report("scale1_partition_independence_19ns", Clock::QueryGuestTickCount(), 47500ull);
  for (auto scale : {0.5, 1.0, 1.25, 0.75, 2.0}) {
    reset(scale);
    host_ticks = 1000000000;
    std::string tag = "scale" + std::to_string(scale);
    report((tag + "_one_second").c_str(), Clock::QueryGuestTickCount(), uint64_t(50000000 * scale));
    report((tag + "_1000ms_to_host").c_str(), Clock::ScaleGuestDurationMillis(1000),
           uint32_t(1000 / scale));
    using namespace rex::chrono;
    auto guest_future = XSystemClock::now() + std::chrono::seconds(1);
    auto host_future = std::chrono::clock_cast<WinSystemClock>(guest_future);
    report((tag + "_guest_deadline_to_host_100ns").c_str(),
           (host_future - WinSystemClock::now()).count(), int64_t(10000000 / scale));
    auto host_future2 = WinSystemClock::now() + std::chrono::seconds(1);
    auto guest_future2 = std::chrono::clock_cast<XSystemClock>(host_future2);
    report((tag + "_host_deadline_to_guest_100ns").c_str(),
           (guest_future2 - XSystemClock::now()).count(), int64_t(10000000 * scale));
  }
  reset();
  for (int64_t value : {-1ll, -100ll, -10000ll, -10000000ll}) {
    report(("scale1_relative_filetime_" + std::to_string(value)).c_str(),
           Clock::ScaleGuestDurationFileTime(value), value);
  }
  reset();
  host_ticks = 1000000000;
  Clock::set_guest_time_scalar(2.0);
  report("scale_change_preserves_elapsed_interval", Clock::QueryGuestTickCount(), 50000000ull);
  reset();
  host_ticks = 1000;
  intercept = true;
  std::thread early([] { Clock::QueryGuestTickCount(); });
  {
    std::unique_lock lock(sample_mutex);
    sample_cv.wait(lock, [] { return sampled; });
  }
  host_ticks = 2000;
  track_second = true;
  std::thread later([] { Clock::QueryGuestTickCount(); });
  {
    std::unique_lock lock(sample_mutex);
    sample_cv.wait_for(lock, std::chrono::milliseconds(100), [] { return second_sampled; });
    second_sampled_before_release = second_sampled;
    released = true;
  }
  sample_cv.notify_all();
  early.join();
  later.join();
  track_second = false;
  std::cerr << "second_sample_while_first_paused=" << second_sampled_before_release << "\n";
  host_ticks = 3000;
  report("out_of_order_sample_no_doublecount", Clock::QueryGuestTickCount(), 150ull);
  reset();
  int32_t sec = 0, usec = 1500000;
  Clock::ScaleGuestDurationTimeval(&sec, &usec);
  report("timeval_carries_seconds", sec, 1);
  report("timeval_normalizes_microseconds", usec, 500000);
  reset();
  report("millis_zero", Clock::ScaleGuestDurationMillis(0), 0u);
  report("millis_infinite", Clock::ScaleGuestDurationMillis(UINT32_MAX), UINT32_MAX);
  reset(0.5);
  report("millis_overflow_saturates", Clock::ScaleGuestDurationMillis(UINT32_MAX - 1), UINT32_MAX);
  report("filetime_negative_overflow_saturates", Clock::ScaleGuestDurationFileTime(INT64_MIN),
         INT64_MIN);
  reset();
  host_ticks = 1000000000;
  report("guest_system_time_elapsed",
         Clock::QueryGuestSystemTime() - Clock::guest_system_time_base(), 10000000ull);
  report("guest_uptime_elapsed", Clock::QueryGuestUptimeMillis(), 1000u);
  clock_no_scaling = true;
  report("no_scaling_counter_frequency", Clock::QueryGuestTickCount(), 50000000ull);
  report("no_scaling_relative_filetime", Clock::ScaleGuestDurationFileTime(-100), -100ll);
  report("no_scaling_host_systemtime", Clock::QueryGuestSystemTime(), Clock::QueryHostSystemTime());
  reset();
  host_ticks = 1000000000;
  auto before = Clock::QueryGuestTickCount();
  host_ticks = 500000000;
  report("backwards_host_no_guest_regression", Clock::QueryGuestTickCount(), before);
  host_ticks = 1500000000;
  report("backwards_host_no_double_count", Clock::QueryGuestTickCount(), 75000000ull);
  reset(3.0);
  host_ticks = UINT64_MAX;
  report("large_interval_exact_no_product_overflow", Clock::QueryGuestTickCount(),
         2767011611056432742ull);
  reset();
  host_ticks = 1000000000;
  Clock::SetGuestSystemTime(0);
  report("set_system_time_zero_after_elapsed", Clock::QueryGuestSystemTime(), 0ull);
  host_ticks = 2000000000;
  report("system_time_progress_after_epoch_reset", Clock::QueryGuestSystemTime(), 10000000ull);
  if (argc > 1) {
    reset();
    auto ratio = Clock::guest_tick_ratio();
    for (double bad :
         {0.0, -1.0, std::numeric_limits<double>::infinity(),
          std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::denorm_min(),
          std::numeric_limits<double>::max()}) {
      Clock::set_guest_time_scalar(bad);
      report("invalid_scalar_preserves_rate", Clock::guest_time_scalar(), 1.0);
      report("invalid_scalar_preserves_ratio", Clock::guest_tick_ratio() == ratio, true);
    }
    Clock::set_guest_tick_frequency(0);
    report("invalid_frequency_preserves_rate", Clock::guest_tick_frequency(), 50000000ull);
    report("invalid_rates_reported", probe_warning_count, 7u);
    reset();
    Clock::set_guest_system_time_base(Clock::QueryHostSystemTime() - 100);
    auto guest_now = Clock::QueryGuestSystemTime();
    auto host_now = Clock::QueryHostSystemTime();
    report("scale1_absolute_future_exact",
           Clock::ScaleGuestDurationFileTime(static_cast<int64_t>(guest_now + 1)),
           static_cast<int64_t>(host_now + 1));
    report("scale1_absolute_past_exact",
           Clock::ScaleGuestDurationFileTime(static_cast<int64_t>(guest_now - 1)),
           static_cast<int64_t>(host_now - 1));
    reset(std::ldexp(1.0, -50));
    using namespace rex::chrono;
    auto distant =
        std::chrono::clock_cast<WinSystemClock>(XSystemClock::now() + std::chrono::seconds(1));
    report("tiny_valid_scale_deadline_saturates", distant.time_since_epoch().count(), INT64_MAX);
    auto distant_past = std::chrono::clock_cast<WinSystemClock>(XSystemClock::time_point{});
    report("tiny_valid_scale_past_saturates", distant_past.time_since_epoch().count(), INT64_MIN);
    reset(std::ldexp(1.0, 50));
    auto distant_guest =
        std::chrono::clock_cast<XSystemClock>(WinSystemClock::now() + std::chrono::seconds(1));
    report("huge_valid_scale_deadline_saturates", distant_guest.time_since_epoch().count(),
           INT64_MAX);
    auto distant_guest_past = std::chrono::clock_cast<XSystemClock>(WinSystemClock::time_point{});
    report("huge_valid_scale_past_saturates", distant_guest_past.time_since_epoch().count(),
           INT64_MIN);
    reset();
    host_ticks = UINT64_MAX;
    auto until = Clock::QueryGuestTickCount();
    report("large_system_time_conversion_no_overflow",
           Clock::QueryGuestSystemTime() - Clock::guest_system_time_base(), until / 5);
  }
  return 0;
}
