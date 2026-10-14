#include <rex/ppc/context.h>
#include <rex/chrono/clock.h>
#include <cassert>
#include <cfenv>
#include <chrono>
#include <cstring>
#include <thread>
#include <cstdio>
extern "C" void sub_823AFA68(PPCContext&, uint8_t*);
extern "C" void sub_82190990(PPCContext&, uint8_t*);
extern "C" void sub_821908D0(PPCContext&, uint8_t*);
extern "C" void sub_8212E0C8(PPCContext&, uint8_t*);
extern "C" void sub_8249D668(PPCContext&, uint8_t*);
extern "C" void sub_821DCD78(PPCContext&, uint8_t*);
extern "C" void sub_822D0F50(PPCContext&, uint8_t*);
static int calls[7]{};
static uint32_t start_result = 1;
static uint8_t memory[1]{};
static void check(PPCContext& ctx, uint8_t* base, int slot) {
  assert(base == memory);
  assert(std::fegetround() == FE_UPWARD);
  assert(std::fetestexcept(FE_ALL_EXCEPT) == FE_DIVBYZERO);
  ++calls[slot];
  ctx.r7.u64 = 0x0123456789abcdef;
}
extern "C" void __imp__sub_823AFA68(PPCContext& c, uint8_t* b) {
  check(c, b, 0);
}
extern "C" void __imp__sub_821908D0(PPCContext& c, uint8_t* b) {
  check(c, b, 2);
}
extern "C" void __imp__sub_82190990(PPCContext& c, uint8_t* b) {
  check(c, b, 1);
  sub_821908D0(c, b);
  c.r3.u64 = start_result;
}
extern "C" void __imp__sub_8212E0C8(PPCContext& c, uint8_t* b) {
  check(c, b, 3);
}
extern "C" void __imp__sub_8249D668(PPCContext& c, uint8_t* b) {
  check(c, b, 4);
  c.r3.u64 = 0;
  c.r5.s64 = -2;
  c.r7.s64 = 1234;
}
extern "C" void __imp__sub_821DCD78(PPCContext& c, uint8_t* b) {
  check(c, b, 5);
  assert(c.r3.u32 == 42 && c.r4.u32 == 4);
}
extern "C" void __imp__sub_822D0F50(PPCContext& c, uint8_t* b) {
  check(c, b, 6);
}
static void run(void (*f)(PPCContext&, uint8_t*), bool start = false, bool deadline = false) {
  PPCContext c{};
  c.r3.u64 = 42;
  c.r4.u64 = 4;
  c.f1.f64 = 0.125;
  PPCContext expected = c;
  expected.r7.u64 = 0x0123456789abcdef;
  if (start)
    expected.r3.u64 = start_result;
  if (deadline) {
    expected.r3.u64 = 0;
    expected.r5.s64 = -2;
    expected.r7.s64 = 1234;
  }
  std::fesetround(FE_UPWARD);
  std::feclearexcept(FE_ALL_EXCEPT);
  std::feraiseexcept(FE_DIVBYZERO);
  f(c, memory);
  assert(std::memcmp(&c, &expected, sizeof(c)) == 0);
  assert(std::fegetround() == FE_UPWARD);
  assert(std::fetestexcept(FE_ALL_EXCEPT) == FE_DIVBYZERO);
}
int main() {
  rex::chrono::Clock::set_guest_tick_frequency(50000000);
  run(sub_823AFA68);
  run(sub_821908D0);
  run(sub_82190990, true);
  run(sub_821908D0);
  start_result = 0;
  run(sub_82190990, true);
  run(sub_821908D0);
  run(sub_8212E0C8);
  run(sub_822D0F50);
  std::this_thread::sleep_for(std::chrono::milliseconds(1020));
  run(sub_8212E0C8);
  run(sub_8249D668, false, true);
  run(sub_821DCD78);
  run(sub_822D0F50);
  assert(calls[5] == 1 && calls[6] == 2);
  assert(calls[4] == 1);
  assert(calls[0] == 1 && calls[1] == 2 && calls[2] == 5 && calls[3] == 2);
  std::puts(
      "Forwarding, register preservation, nested/failed start and FP environment checks passed.");
}
