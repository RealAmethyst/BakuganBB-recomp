// Original synthetic guest-memory fixtures. No game code or devices execute.
#include "native_gate_audio.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <bit>
#include <cfenv>
#include <cstring>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include <rex/ppc/context.h>
#include <rex/thread/mutex.h>

#define HOOK(address) extern "C" void sub_##address(PPCContext&, uint8_t*)
HOOK(82218AF0); HOOK(82216760); HOOK(82217C50); HOOK(822180A8); HOOK(82216F78);
HOOK(8226D8C0); HOOK(82222AC0); HOOK(82222638); HOOK(82222710);
#undef HOOK

std::recursive_mutex& rex::thread::global_critical_region::mutex() {
  static std::recursive_mutex mutex;
  return mutex;
}

namespace {
using namespace brawlers;
constexpr uint32_t kScene = 0x10000, kContainer = kScene + 0x1FE0;
constexpr uint32_t kProxy = 0x13000, kCards = 0x14000, kSource = 0x18000;
constexpr uint32_t kModels = 0x20000, kTask = 0x19000;
using Hook = void (*)(PPCContext&, uint8_t*);
std::function<void(uint32_t, PPCContext&)> native;
size_t forwarded = 0;
void Forward(uint32_t address, PPCContext& ctx) {
  ++forwarded;
  if (native) native(address, ctx);
}

struct Fixture {
  std::array<uint8_t, 0x30000> bytes{};
  uint64_t generation = 1;
  audio::Scene scene;
  bool published = false, flip_rounding = false, reject = false, throws = false;
  bool invalidate_read = false;
  uint32_t unreadable = 0;
  size_t publications = 0, invalidations = 0, reads = 0;
  std::vector<std::string> guards;

  Fixture() {
    native = {};
    game::SetGateAudioCapture([this](uint32_t at, void* out, size_t size) {
      ++reads;
      if (flip_rounding) std::fesetround(FE_UPWARD);
      if (throws) throw std::runtime_error("synthetic read failure");
      if (invalidate_read) { invalidate_read = false; ++generation; published = false; }
      if (uint64_t(at) + size > bytes.size() ||
          (unreadable && at <= unreadable && unreadable < uint64_t(at) + size)) return false;
      std::memcpy(out, bytes.data() + at, size);
      return true;
    }, {this,
      [](void* p) noexcept { return static_cast<Fixture*>(p)->generation; },
      [](void* p, const audio::Scene& scene, uint64_t token) noexcept {
        auto& f = *static_cast<Fixture*>(p);
        if (f.reject || f.generation != token) return false;
        assert(audio::ValidateScene(scene) == audio::Diagnostic::Count);
        f.scene = scene; f.published = true; ++f.publications; return true;
      },
      [](void* p) noexcept {
        auto& f = *static_cast<Fixture*>(p);
        ++f.generation; ++f.invalidations; f.published = false;
        if (f.flip_rounding) std::fesetround(FE_UPWARD);
      },
      [](void* p, std::string_view guard) noexcept {
        static_cast<Fixture*>(p)->guards.emplace_back(guard);
      }});
    Byte(kScene + 0x24DF, 1); Byte(kScene + 0x24E0, 1);
    Byte(kScene + 0x24C5, 2);
    Byte(kScene + 0x1D54, 0); Byte(kScene + 0x1D54 + 0xD8, 1);
    U32(kContainer, kProxy); U32(kProxy, kContainer);
    U32(kContainer + 12, kCards); U32(kContainer + 16, kCards);
    U32(kContainer + 20, kCards + 32 * 0x40);
    U32(kScene + 0x224, kModels); U32(kScene + 0x228, kModels + 4 * 0x190);
    U32(kScene + 0x560, 0x820079B0);
    Camera({1, 2, 3}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1});
    std::fill_n(bytes.data() + kScene + 0x23BC, 256, 0xFF);
    Card(kCards, 1, 0, 0, {7, 0, 0});
    Card(kCards + 0x40, 2, 1, 1, {-7, 0, 0});
    Count(2);
    Call(sub_82218AF0, kScene);
  }
  ~Fixture() { native = {}; game::SetGateAudioCapture({}, {}); }

  void Byte(uint32_t at, uint8_t value) { bytes.at(at) = value; }
  void U32(uint32_t at, uint32_t value) {
    for (size_t i = 0; i < 4; ++i) bytes.at(at + i) = uint8_t(value >> (24 - 8 * i));
  }
  void F32(uint32_t at, float value) { U32(at, std::bit_cast<uint32_t>(value)); }
  void Vec(uint32_t at, audio::Vec3 v) { F32(at, v.x); F32(at + 4, v.y); F32(at + 8, v.z); }
  void Camera(audio::Vec3 eye, audio::Vec3 right, audio::Vec3 up, audio::Vec3 back) {
    Vec(kScene + 0x580, eye);
    const std::array<audio::Vec3, 3> axes{right, up, back};
    for (size_t i = 0; i < 3; ++i) {
      F32(kScene + 0x650 + i * 4, axes[i].x);
      F32(kScene + 0x660 + i * 4, axes[i].y);
      F32(kScene + 0x670 + i * 4, axes[i].z);
      F32(kScene + 0x680 + i * 4,
          -(axes[i].x * eye.x + axes[i].y * eye.y + axes[i].z * eye.z));
    }
    F32(kScene + 0x68C, 1);
  }
  void Card(uint32_t at, uint8_t cell, uint8_t owner, uint8_t model, audio::Vec3 pos) {
    std::fill_n(bytes.data() + at, 0x40, 0);
    Vec(at, pos); F32(at + 0x28, 1);
    Byte(at + 0x2C, model); Byte(at + 0x2D, owner);
    Byte(at + 0x2E, cell % 16); Byte(at + 0x2F, cell / 16);
  }
  void Count(size_t count) {
    U32(kContainer + 16, kCards + uint32_t(count * 0x40));
    std::fill_n(bytes.data() + kScene + 0x23BC, 256, 0xFF);
    for (size_t i = 0; i < count; ++i) {
      const auto at = kCards + uint32_t(i * 0x40);
      Byte(kScene + 0x23BC + bytes[at + 0x2E] + 16 * bytes[at + 0x2F], uint8_t(i));
    }
  }
  void Enable() { game::ConfigureGateAudioCapture(game::GateAudioActivation::NativeCardRendering); }
  void Draw() { Call(sub_82216760, kScene); }
  bool Guard(std::string_view name) const {
    return std::find(guards.begin(), guards.end(), name) != guards.end();
  }
  static PPCContext Call(Hook hook, uint64_t r3, uint64_t r4 = 0, uint64_t r5 = 0,
                         uint64_t r6 = 0) {
    PPCContext ctx{};
    ctx.r3.u64 = r3; ctx.r4.u64 = r4; ctx.r5.u64 = r5; ctx.r6.u64 = r6;
    ctx.r9.u64 = 0xFEDCBA9876543210;
    const auto before = forwarded;
    hook(ctx, nullptr);
    assert(forwarded > before);
    assert(ctx.r9.u64 == 0xFEDCBA9876543210);
    return ctx;
  }
  void EraseFirst() {
    native = [&](uint32_t address, PPCContext&) {
      if (address != 0x82222638) return;
      std::memmove(bytes.data() + kCards, bytes.data() + kCards + 0x40, 0x40);
      Count(1);
    };
    Call(sub_82222638, kSource, kContainer, uint64_t(kProxy) << 32 | kCards);
    native = {};
  }
};

void LifecycleAndPolicy() {
  Fixture f;
  f.Draw(); assert(!f.published && f.reads == 0);
  f.Enable(); f.Draw();
  assert(f.published && f.scene.count == 2);
  assert(f.scene.sources[0].cue == audio::Cue::Friendly);
  assert(f.scene.sources[1].cue == audio::Cue::Enemy);
  const auto id = f.scene.sources[0].id;
  f.Vec(kCards, {12, 0, 4}); f.Draw();
  assert(f.scene.sources[0].id == id && f.scene.sources[0].position.x == 12);
  f.Camera({0, 0, 0}, {0, 0, 1}, {0, 1, 0}, {-1, 0, 0}); f.Draw();
  assert(f.scene.listener_ahead.x == 1);
  f.Byte(kScene + 0x24E1, 1); f.Draw(); assert(!f.published);
  f.Byte(kScene + 0x24E1, 0); f.Draw(); assert(f.published);
  f.U32(kTask + 0x18, 1); Fixture::Call(sub_82216F78, kTask); assert(!f.published);
  f.Draw();
  game::ConfigureGateAudioCapture(game::GateAudioActivation::Disabled);
  assert(!f.published); f.Draw(); assert(!f.published);
  f.Enable(); f.Draw();
  Fixture::Call(sub_82217C50, kScene); assert(!f.published);
  f.Draw(); assert(!f.published && f.Guard("missing_scene_lifetime"));
  Fixture::Call(sub_82218AF0, kScene); f.Draw();
  assert(f.published && f.scene.sources[0].id != id);
}

void IdentityAcrossMutations() {
  Fixture f; f.Enable(); f.Draw();
  const auto removed = f.scene.sources[0].id, survivor = f.scene.sources[1].id;
  f.EraseFirst();
  assert(f.published && f.scene.count == 1 && f.scene.sources[0].id == survivor);
  f.Card(kSource, 1, 0, 0, {7, 0, 0});
  native = [&](uint32_t address, PPCContext&) {
    if (address != 0x8226D8C0) return;
    // Capacity-growth insertion passes through the generic insert as well.
    Fixture::Call(sub_82222AC0, kContainer, 0, 1, kSource);
    std::memcpy(f.bytes.data() + kCards + 0x40, f.bytes.data() + kSource, 0x40);
    f.Count(2);
  };
  Fixture::Call(sub_8226D8C0, kContainer, kSource); native = {};
  f.Draw();
  assert(f.scene.sources[0].id == survivor && f.scene.sources[1].id != removed);
  const auto inserted = f.scene.sources[1].id;
  // Generic erasure covers clear and ranges; surviving source remains unchanged.
  Fixture::Call(sub_82222710, kSource, kContainer,
                uint64_t(kProxy) << 32 | (kCards + 0x40),
                uint64_t(kProxy) << 32 | (kCards + 0x80));
  f.Count(1); assert(f.scene.count == 1 && f.scene.sources[0].id == survivor);
  Fixture::Call(sub_82222AC0, kContainer, 0, 1, kSource);
  std::memcpy(f.bytes.data() + kCards + 0x40, f.bytes.data() + kSource, 0x40);
  f.Count(2); f.Draw();
  assert(f.scene.sources[1].id != inserted && f.scene.sources[0].id == survivor);
  // A different vector's erase must not affect this scene.
  Fixture::Call(sub_82222638, kSource, kContainer + 24, 0); f.Draw();
  assert(f.scene.sources[0].id == survivor);
  Fixture::Call(sub_822180A8, kScene); f.Draw();
  assert(f.scene.sources[0].id != survivor);
}

void InvalidInput() {
  const auto check = [](const std::function<void(Fixture&)>& corrupt, const char* guard) {
    Fixture f; f.Enable(); f.Draw(); assert(f.published);
    corrupt(f); f.Draw(); assert(!f.published && f.Guard(guard));
  };
  check([](Fixture& f) { f.U32(kProxy, kContainer + 4); }, "container_proxy");
  check([](Fixture& f) { f.U32(kContainer + 16, kCards + 65); }, "container_bounds");
  check([](Fixture& f) { f.U32(kContainer + 20, kCards + 64); }, "container_bounds");
  check([](Fixture& f) { f.U32(kContainer + 16, kCards + 33 * 64);
                        f.U32(kContainer + 20, kCards + 33 * 64); }, "source_capacity");
  check([](Fixture& f) { f.U32(kContainer + 20, kCards + 257 * 64); }, "container_allocation");
  check([](Fixture& f) { f.unreadable = kCards + 10; }, "guest_read");
  check([](Fixture& f) { f.Byte(kScene + 0x24C5, 5); }, "participant_count");
  check([](Fixture& f) { f.Byte(kScene + 0x1D54 + 0xD8, 0); }, "unsupported_participant_setup");
  check([](Fixture& f) { f.Byte(kScene + 0x1D54, 1); }, "unsupported_participant_setup");
  check([](Fixture& f) { f.Byte(kScene + 0x24C9, 1); }, "unsupported_relationship_mode");
  check([](Fixture& f) { f.Byte(kScene + 0x24C9, 7); }, "invalid_relationship_mode");
  check([](Fixture& f) { f.Byte(kCards + 0x2D, 2); }, "source_owner");
  check([](Fixture& f) { f.Byte(kCards + 0x2E, 16); }, "source_grid");
  check([](Fixture& f) { f.Byte(kCards + 0x40 + 0x2E, 1); }, "duplicate_grid");
  check([](Fixture& f) { f.Byte(kScene + 0x23BD, 3); }, "grid_mapping");
  check([](Fixture& f) { f.Byte(kCards + 0x2C, 4); }, "model_index");
  check([](Fixture& f) { f.U32(kScene + 0x560, 1); }, "camera_type");
  check([](Fixture& f) { f.F32(kScene + 0x650, -1); }, "camera_basis");
  check([](Fixture& f) { f.F32(kScene + 0x580, 100); }, "camera_basis");
  check([](Fixture& f) { f.F32(kCards, std::numeric_limits<float>::infinity()); }, "source_position");
  // Empty/default participant rows are not counted beyond the native active count.
  Fixture f; f.Enable(); f.Draw(); assert(f.published);
  f.Count(0); f.Draw(); assert(f.published && f.scene.count == 0);
}

void GenerationAndForwarding() {
  Fixture f; f.Enable(); f.Draw();
  ++f.generation; f.published = false;
  f.EraseFirst(); assert(!f.published);
  f.Draw(); assert(f.published && f.scene.count == 1);
  f.invalidate_read = true; f.Draw(); assert(!f.published);
  f.Draw(); assert(f.published);
  f.flip_rounding = true;
  std::fesetround(FE_DOWNWARD);
  native = [&](uint32_t address, PPCContext& ctx) {
    assert(address == 0x82216760 && ctx.r3.u32 == kScene);
    assert(std::fegetround() == FE_DOWNWARD);
    ctx.r3.u64 = 0x123456789ABCDEF0;
    std::fesetround(FE_TOWARDZERO);
  };
  auto ctx = Fixture::Call(sub_82216760, kScene);
  assert(ctx.r3.u64 == 0x123456789ABCDEF0 && std::fegetround() == FE_TOWARDZERO);
  native = {};
  f.throws = true; f.Draw();
  assert(!f.published && f.Guard("capture_exception"));
  f.throws = false; f.Draw(); assert(!f.published);
  Fixture::Call(sub_822180A8, kScene); f.Draw(); assert(f.published);
  // An unreadable mutation poisons the lifetime instead of silently reusing IDs.
  f.unreadable = kSource;
  Fixture::Call(sub_8226D8C0, kContainer, kSource);
  f.unreadable = 0; f.Draw(); assert(!f.published);
  Fixture::Call(sub_822180A8, kScene); f.Draw(); assert(f.published);
  f.flip_rounding = false;
  std::fesetround(FE_TONEAREST);
}
}  // namespace

#define STUB(address) \
  extern "C" void __imp__sub_##address(PPCContext& ctx, uint8_t*) { Forward(0x##address, ctx); }
STUB(82218AF0); STUB(82216760); STUB(82217C50); STUB(822180A8); STUB(82216F78);
STUB(8226D8C0); STUB(82222AC0); STUB(82222638); STUB(82222710);
#undef STUB

int main() {
  LifecycleAndPolicy();
  IdentityAcrossMutations();
  InvalidInput();
  GenerationAndForwarding();
}
