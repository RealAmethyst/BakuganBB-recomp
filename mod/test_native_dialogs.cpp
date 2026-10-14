// Original synthetic contexts only. No guest executable or input devices.
#include <rex/ppc/context.h>

#include <cassert>
#include <cfenv>
#include <cstdio>
#include <cstring>

extern "C" void sub_823BC4F0(PPCContext&, uint8_t*);
extern "C" void sub_823B6A40(PPCContext&, uint8_t*);

namespace {
uint8_t memory[1]{};
PPCContext incoming{};
unsigned request_calls = 0, poll_calls = 0;
uint32_t result = 0;
bool nested = false;

void CheckInput(PPCContext& ctx, uint8_t* base) {
  assert(base == memory);
  assert(std::memcmp(&ctx, &incoming, sizeof(ctx)) == 0);
  assert(std::fegetround() == FE_UPWARD);
  assert(std::fetestexcept(FE_ALL_EXCEPT) == FE_DIVBYZERO);
}

void SetOutput(PPCContext& ctx) {
  ctx.r3.u64 = result;
  ctx.r7.u64 = 0xabcdef0123456789;
  ctx.lr = 0x12345678;
  std::fesetround(FE_DOWNWARD);
  std::feclearexcept(FE_ALL_EXCEPT);
  std::feraiseexcept(FE_INVALID);
}

void Run(void (*function)(PPCContext&, uint8_t*), uint32_t value) {
  result = value;
  PPCContext ctx = incoming;
  PPCContext expected = incoming;
  SetOutput(expected);
  std::fesetround(FE_UPWARD);
  std::feclearexcept(FE_ALL_EXCEPT);
  std::feraiseexcept(FE_DIVBYZERO);
  function(ctx, memory);
  assert(std::memcmp(&ctx, &expected, sizeof(ctx)) == 0);
  assert(std::fegetround() == FE_DOWNWARD);
  assert(std::fetestexcept(FE_ALL_EXCEPT) == FE_INVALID);
}
}  // namespace

extern "C" void __imp__sub_823BC4F0(PPCContext& ctx, uint8_t* base) {
  CheckInput(ctx, base);
  ++request_calls;
  if (nested) {
    nested = false;
    PPCContext other = ctx;
    sub_823B6A40(other, base);
  }
  SetOutput(ctx);
}

extern "C" void __imp__sub_823B6A40(PPCContext& ctx, uint8_t* base) {
  CheckInput(ctx, base);
  ++poll_calls;
  SetOutput(ctx);
}

int main() {
  incoming.r3.u64 = 0xff;
  incoming.r4.u64 = 4;
  incoming.r5.u64 = 0xffffffff;  // Unreadable text pointers must remain opaque.
  incoming.r6.u64 = 0xfffffffe;
  incoming.r7.u64 = 0xfffffffd;
  incoming.r8.u64 = 0xfffffffc;
  incoming.r9.u64 = 11;
  incoming.r10.u64 = 0x12340000;
  incoming.lr = 0x823b7898;
  incoming.f1.f64 = 0.125;
  Run(sub_823BC4F0, 0x3e5);
  Run(sub_823BC4F0, 0x57);
  for (unsigned i = 0; i < 100; ++i)
    Run(sub_823B6A40, 1);
  Run(sub_823B6A40, 2);
  Run(sub_823B6A40, 3);
  Run(sub_823B6A40, 0);
  nested = true;
  Run(sub_823BC4F0, 0x3e5);
  assert(request_calls == 3 && poll_calls == 104);
  std::puts("107 calls: input/output contexts, opaque pointers, nested calls and FP state passed.");
}
