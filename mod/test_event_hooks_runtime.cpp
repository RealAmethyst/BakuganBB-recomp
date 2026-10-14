// Executes only the original synthetic bodies emitted by test_event_hooks_codegen.
#include <rex/ppc/context.h>

#include <array>
#include <cassert>
#include <cfenv>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {
alignas(64) std::array<uint8_t, 64> memory;
uint32_t observed_value, observed_address, observed_memory;
unsigned observations;
constexpr uint32_t destination = 16;
constexpr uint32_t sentinel = 0x76543210;

uint32_t Load(uint32_t address) {
  assert(address <= memory.size() - 4);
  uint32_t value = 0;
  for (unsigned i = 0; i < 4; ++i) value = (value << 8) | memory[address + i];
  return value;
}

void Store(uint8_t* base, uint32_t address, uint32_t value) {
  assert(base == memory.data() && address <= memory.size() - 4);
  for (unsigned i = 0; i < 4; ++i)
    base[address + i] = static_cast<uint8_t>(value >> (24 - i * 8));
}

void Capture(PPCRegister& value, PPCRegister& address) {
  ++observations;
  observed_value = value.u32;
  observed_address = address.u32;
  observed_memory = Load(address.u32);
}
}  // namespace

// Minimal synthetic memory scaffolding: no runtime initialization or guest mappings.
#define DEFINE_REX_FUNC(name) REX_FUNC(name)
#define REX_FUNC_PROLOGUE() ((void)0)
#define REX_STORE_U32(address, value) Store(base, address, value)

namespace context_before {
#include "context_before.cpp"
void SyntheticStoreEvent(PPCRegister& value, PPCRegister& address) { Capture(value, address); }
}
namespace context_after {
#include "context_after.cpp"
void SyntheticStoreEvent(PPCRegister& value, PPCRegister& address) { Capture(value, address); }
}
namespace local_before {
#include "local_before.cpp"
void SyntheticStoreEvent(PPCRegister& value, PPCRegister& address) { Capture(value, address); }
}
namespace local_after {
#include "local_after.cpp"
void SyntheticStoreEvent(PPCRegister& value, PPCRegister& address) { Capture(value, address); }
}

int main() {
  const std::array<PPCFunc*, 4> functions = {
      context_before::sub_82000000, context_after::sub_82000000,
      local_before::sub_82000000, local_after::sub_82000000};
  for (unsigned index = 0; index < functions.size(); ++index) {
    PPCContext context;
    std::memset(&context, 0xA5, sizeof(context));
    context.r3.u64 = 0x12345678;
    context.r4.u64 = destination;
    context.r5.u64 = 0xFEDCBA9876543210;
    context.r30.u64 = 0xAABBCCDDEEFF0011;
    PPCContext expected;
    std::memcpy(&expected, &context, sizeof(context));
    expected.r5.u64 = context.r3.u64 + 1;
    if (index < 2) expected.r30.u64 = expected.r5.u64;
    memory.fill(0xCC);
    Store(memory.data(), destination, sentinel);
    observations = 0;
    assert(std::fesetround(FE_DOWNWARD) == 0);
    assert(std::feclearexcept(FE_ALL_EXCEPT) == 0);
    assert(std::feraiseexcept(FE_INVALID | FE_DIVBYZERO) == 0);
    const auto exceptions = std::fetestexcept(FE_ALL_EXCEPT);
    functions[index](context, memory.data());
    assert(observations == 1);
    assert(observed_value == expected.r5.u32 && observed_address == destination);
    assert(observed_memory == (index % 2 ? expected.r5.u32 : sentinel));
    assert(Load(destination) == expected.r5.u32);
    assert(std::memcmp(&context, &expected, sizeof(context)) == 0);
    assert(std::fegetround() == FE_DOWNWARD);
    assert(std::fetestexcept(FE_ALL_EXCEPT) == exceptions);
    for (unsigned i = 0; i < memory.size(); ++i)
      if (i < destination || i >= destination + 4) assert(memory[i] == 0xCC);
  }
  std::puts("Four generated store hooks passed: order, live/local registers, context and FP preservation.");
}
