#include "native_gate_audio.h"

#include <array>
#include <bit>
#include <cfenv>
#include <cmath>
#include <limits>
#include <utility>

#include <rex/ppc/context.h>
#include <rex/thread/mutex.h>

#define ORIGINAL(address) extern "C" void __imp__sub_##address(PPCContext&, uint8_t*)
ORIGINAL(82218AF0);
ORIGINAL(82216760);
ORIGINAL(82217C50);
ORIGINAL(822180A8);
ORIGINAL(82216F78);
ORIGINAL(8226D8C0);
ORIGINAL(82222AC0);
ORIGINAL(82222638);
ORIGINAL(82222710);
#undef ORIGINAL

namespace {
using brawlers::audio::Scene;
using brawlers::audio::Vec3;
using brawlers::game::GateAudioActivation;
using brawlers::game::GateAudioOutput;

constexpr uint32_t kContainer = 0x1FE0;
constexpr size_t kStride = 0x40;
constexpr size_t kCells = 256;
constexpr size_t kAllocationLimit = kCells * kStride;

enum class Guard : size_t {
  SceneAddress, MissingLifetime, DuplicateLifetime, Reader, ContainerProxy,
  ContainerBounds, ContainerAllocation, SourceCapacity, ParticipantCount,
  ParticipantRead, UnsupportedParticipants, RelationshipRead,
  UnsupportedRelationship, InvalidRelationship, RenderGate, CameraType,
  CameraRead, CameraBasis, Position, Owner, Grid, DuplicateGrid, GridMapping,
  ModelBounds, ModelIndex, Iterator, RangeInsert, TaskRead, IdentityExhausted,
  Exception, Count
};
constexpr std::array<const char*, size_t(Guard::Count)> kGuards{
    "scene_address", "missing_scene_lifetime", "duplicate_scene_lifetime",
    "guest_read", "container_proxy", "container_bounds", "container_allocation",
    "source_capacity", "participant_count", "participant_read",
    "unsupported_participant_setup", "relationship_read",
    "unsupported_relationship_mode", "invalid_relationship_mode", "render_gate",
    "camera_type", "camera_read", "camera_basis", "source_position", "source_owner",
    "source_grid", "duplicate_grid", "grid_mapping", "model_bounds", "model_index",
    "mutation_iterator", "unsupported_range_insert", "render_task_read",
    "identity_exhausted", "capture_exception"};

brawlers::ui::ReadBytes reader;
GateAudioOutput output;
rex::thread::global_critical_region region;
GateAudioActivation activation = GateAudioActivation::Disabled;
uint32_t scene_address = 0;
uint64_t next_identity = 0;
uint64_t reported = 0;
bool poisoned = false;
size_t resetting = 0;
std::array<uint64_t, kCells> identities{};
Scene cached_scene;
uint64_t cached_generation = 0;
bool cached = false;

class FloatEnvironment {
 public:
  FloatEnvironment() : valid_(std::fegetenv(&value_) == 0) {}
  ~FloatEnvironment() { if (valid_) std::fesetenv(&value_); }
 private:
  std::fenv_t value_;
  bool valid_;
};

void Stop() noexcept {
  cached = false;
  if (output.invalidate) output.invalidate(output.context);
}

bool Check(bool valid, Guard reason) noexcept {
  if (valid) return true;
  const auto bit = uint64_t{1} << size_t(reason);
  if (!(reported & bit)) {
    reported |= bit;
    if (output.diagnostic) output.diagnostic(output.context, kGuards[size_t(reason)]);
  }
  return false;
}

template <typename Function>
void Observe(Function function) noexcept {
  FloatEnvironment preserve;
  try {
    auto lock = region.Acquire();
    try {
      function();
    } catch (...) {
      poisoned = true;
      Check(false, Guard::Exception);
      Stop();
    }
  } catch (...) {
    // Do not bypass a failed guest critical lock with an unprotected read.
    if (output.invalidate) output.invalidate(output.context);
  }
}

uint64_t Generation() noexcept {
  return output.capture_generation ? output.capture_generation(output.context) : 0;
}

bool Read(uint64_t address, void* destination, size_t bytes, Guard reason) {
  constexpr uint64_t limit = std::numeric_limits<uint32_t>::max();
  return Check(address && address <= limit && bytes && bytes - 1 <= limit - address &&
                   reader && reader(uint32_t(address), destination, bytes), reason);
}

uint32_t Word(const uint8_t* bytes) noexcept {
  return uint32_t(bytes[0]) << 24 | uint32_t(bytes[1]) << 16 |
         uint32_t(bytes[2]) << 8 | bytes[3];
}

bool U32(uint64_t address, uint32_t& value, Guard reason) {
  std::array<uint8_t, 4> bytes{};
  if (!Check(address % 4 == 0, reason) || !Read(address, bytes.data(), bytes.size(), reason))
    return false;
  value = Word(bytes.data());
  return true;
}

float Float(const uint8_t* bytes) noexcept { return std::bit_cast<float>(Word(bytes)); }
Vec3 Vector(const uint8_t* bytes) noexcept {
  return {Float(bytes), Float(bytes + 4), Float(bytes + 8)};
}
bool Finite(Vec3 value) noexcept {
  return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
double Dot(Vec3 a, Vec3 b) noexcept {
  return double(a.x) * b.x + double(a.y) * b.y + double(a.z) * b.z;
}

struct Cards {
  uint32_t proxy = 0, begin = 0, end = 0, capacity = 0;
  size_t count = 0;
  std::array<uint8_t, kAllocationLimit> bytes{};
  const uint8_t* At(size_t index) const { return bytes.data() + index * kStride; }
};

bool ReadCards(Cards& cards) {
  std::array<uint8_t, 24> header{};
  const auto container = uint64_t(scene_address) + kContainer;
  if (!Read(container, header.data(), header.size(), Guard::Reader)) return false;
  cards.proxy = Word(header.data());
  cards.begin = Word(header.data() + 12);
  cards.end = Word(header.data() + 16);
  cards.capacity = Word(header.data() + 20);
  uint32_t owner = 0;
  if (!U32(cards.proxy, owner, Guard::ContainerProxy) ||
      !Check(owner == container, Guard::ContainerProxy)) return false;
  if (!Check(cards.begin <= cards.end && cards.end <= cards.capacity &&
                 cards.begin % 16 == 0 && (cards.end - cards.begin) % kStride == 0 &&
                 (cards.capacity - cards.begin) % kStride == 0 &&
                 (cards.begin || (!cards.end && !cards.capacity)),
             Guard::ContainerBounds)) return false;
  const auto allocation = cards.capacity - cards.begin;
  if (!Check(allocation <= kAllocationLimit, Guard::ContainerAllocation)) return false;
  cards.count = (cards.end - cards.begin) / kStride;
  if (!Check(cards.count <= brawlers::audio::kMaxSources, Guard::SourceCapacity)) return false;
  return !allocation || Read(cards.begin, cards.bytes.data(), allocation, Guard::Reader);
}

bool Cell(const uint8_t* record, uint8_t& cell) noexcept {
  if (!Check(record[0x2E] < 16 && record[0x2F] < 16, Guard::Grid)) return false;
  cell = uint8_t(record[0x2F] * 16 + record[0x2E]);
  return true;
}

bool Listener(Scene& scene) {
  uint32_t table = 0;
  if (!U32(uint64_t(scene_address) + 0x560, table, Guard::CameraType) ||
      !Check(table == 0x820079B0, Guard::CameraType)) return false;
  std::array<uint8_t, 12> eye{};
  std::array<uint8_t, 64> matrix{};
  if (!Read(uint64_t(scene_address) + 0x580, eye.data(), eye.size(), Guard::CameraRead) ||
      !Read(uint64_t(scene_address) + 0x650, matrix.data(), matrix.size(), Guard::CameraRead))
    return false;
  scene.listener_position = Vector(eye.data());
  const Vec3 right{Float(matrix.data()), Float(matrix.data() + 16), Float(matrix.data() + 32)};
  scene.listener_up = {Float(matrix.data() + 4), Float(matrix.data() + 20),
                       Float(matrix.data() + 36)};
  const Vec3 back{Float(matrix.data() + 8), Float(matrix.data() + 24),
                  Float(matrix.data() + 40)};
  scene.listener_ahead = {-back.x, -back.y, -back.z};
  const auto up = scene.listener_up;
  const Vec3 cross{up.y * back.z - up.z * back.y, up.z * back.x - up.x * back.z,
                   up.x * back.y - up.y * back.x};
  for (size_t i = 0; i < 16; ++i)
    if (!Check(std::isfinite(Float(matrix.data() + i * 4)), Guard::CameraBasis)) return false;
  const auto eye_position = scene.listener_position;
  const auto translation_matches = [&](size_t offset, Vec3 axis) {
    const auto expected = -Dot(axis, eye_position);
    return std::abs(double(Float(matrix.data() + offset)) - expected) <=
           0.001 * (1 + std::abs(expected));
  };
  return Check(Finite(eye_position) && Finite(right) && Finite(up) && Finite(back) &&
                   std::abs(Dot(right, right) - 1) <= 0.001 &&
                   std::abs(Dot(up, up) - 1) <= 0.001 &&
                   std::abs(Dot(back, back) - 1) <= 0.001 &&
                   std::abs(Dot(right, up)) <= 0.001 &&
                   std::abs(Dot(right, back)) <= 0.001 &&
                   std::abs(Dot(up, back)) <= 0.001 && Dot(cross, right) >= 0.999 &&
                   Float(matrix.data() + 12) == 0 && Float(matrix.data() + 28) == 0 &&
                   Float(matrix.data() + 44) == 0 && Float(matrix.data() + 60) == 1 &&
                   translation_matches(48, right) && translation_matches(52, up) &&
                   translation_matches(56, back),
               Guard::CameraBasis);
}

bool Capture(uint64_t token) {
  std::array<uint8_t, 3> gates{};
  if (!Read(uint64_t(scene_address) + 0x24DF, gates.data(), gates.size(), Guard::RenderGate))
    return false;
  if (!gates[0] || !gates[1] || gates[2]) return false;
  uint8_t count = 0, mode = 0;
  if (!Read(uint64_t(scene_address) + 0x24C5, &count, 1, Guard::ParticipantCount) ||
      !Check(count >= 1 && count <= 4, Guard::ParticipantCount) ||
      !Read(uint64_t(scene_address) + 0x24C9, &mode, 1, Guard::RelationshipRead)) return false;
  if (!Check(mode != 1, Guard::UnsupportedRelationship) ||
      !Check(mode == 0, Guard::InvalidRelationship)) return false;
  uint8_t human = 0;
  size_t humans = 0;
  for (uint8_t i = 0; i < count; ++i) {
    uint8_t cpu = 0;
    if (!Read(uint64_t(scene_address) + 0x1D54 + i * 0xD8, &cpu, 1,
              Guard::ParticipantRead)) return false;
    if (!cpu) { human = i; ++humans; }
  }
  if (!Check(humans == 1, Guard::UnsupportedParticipants)) return false;
  Cards cards;
  if (!ReadCards(cards)) return false;
  Scene scene;
  if (!Listener(scene)) return false;
  std::array<uint8_t, kCells> map{};
  if (!Read(uint64_t(scene_address) + 0x23BC, map.data(), map.size(), Guard::GridMapping))
    return false;
  uint32_t models = 0, model_end = 0;
  if (!U32(uint64_t(scene_address) + 0x224, models, Guard::ModelBounds) ||
      !U32(uint64_t(scene_address) + 0x228, model_end, Guard::ModelBounds) ||
      !Check(models && models % 16 == 0 && models <= model_end &&
                 (model_end - models) % 0x190 == 0 &&
                 (model_end - models) / 0x190 <= 256, Guard::ModelBounds)) return false;
  std::array<bool, kCells> seen{};
  auto new_identities = identities;
  for (size_t i = 0; i < cards.count; ++i) {
    const auto* record = cards.At(i);
    uint8_t cell = 0;
    if (!Cell(record, cell) || !Check(!seen[cell], Guard::DuplicateGrid) ||
        !Check(map[cell] == i, Guard::GridMapping) ||
        !Check(record[0x2D] < count, Guard::Owner) ||
        !Check(record[0x2C] < (model_end - models) / 0x190, Guard::ModelIndex)) return false;
    seen[cell] = true;
    const auto position = Vector(record);
    if (!Check(Finite(position), Guard::Position)) return false;
    if (!new_identities[cell]) {
      if (!Check(next_identity != UINT64_MAX, Guard::IdentityExhausted)) return false;
      new_identities[cell] = ++next_identity;
    }
    scene.sources[i] = {new_identities[cell], record[0x2D] == human ?
        brawlers::audio::Cue::Friendly : brawlers::audio::Cue::Enemy, position, 1};
  }
  scene.count = cards.count;
  for (size_t i = 0; i < kCells; ++i)
    if (!seen[i]) new_identities[i] = 0;
  identities = new_identities;
  if (output.publish && output.publish(output.context, scene, token)) {
    cached_scene = scene;
    cached_generation = token;
    cached = true;
  } else {
    cached = false;
  }
  return true;
}

void Retire(const std::array<bool, kCells>& cells, uint64_t token) noexcept {
  if (cached && cached_generation != token) cached = false;
  Scene remaining = cached_scene;
  remaining.count = 0;
  if (cached) {
    for (size_t i = 0; i < cached_scene.count; ++i) {
      bool removed = false;
      for (size_t cell = 0; cell < kCells; ++cell)
        if (cells[cell] && identities[cell] == cached_scene.sources[i].id) removed = true;
      if (!removed) remaining.sources[remaining.count++] = cached_scene.sources[i];
    }
  }
  for (size_t cell = 0; cell < kCells; ++cell)
    if (cells[cell]) identities[cell] = 0;
  if (cached && remaining.count != cached_scene.count) {
    if (output.publish && output.publish(output.context, remaining, token)) cached_scene = remaining;
    else cached = false;
  }
}

bool Tracks(uint32_t container) noexcept {
  return scene_address && !resetting && !poisoned &&
         uint64_t(container) == uint64_t(scene_address) + kContainer;
}

void Insert(uint32_t container, uint32_t source, uint32_t count) {
  if (!Tracks(container) || !count) return;
  const auto token = Generation();
  std::array<uint8_t, kStride> record{};
  uint8_t cell = 0;
  if (!Check(count == 1, Guard::RangeInsert) ||
      !Read(source, record.data(), record.size(), Guard::Reader) || !Cell(record.data(), cell)) {
    poisoned = true;
    Stop();
    return;
  }
  std::array<bool, kCells> cells{};
  cells[cell] = true;
  Retire(cells, token);
}

void Erase(uint32_t container, uint64_t first, uint64_t last, bool single) {
  if (!Tracks(container)) return;
  const auto token = Generation();
  Cards cards;
  if (!ReadCards(cards)) { poisoned = true; Stop(); return; }
  const auto begin = uint32_t(first);
  const uint64_t end = single ? uint64_t(begin) + kStride : uint32_t(last);
  if (!Check(uint32_t(first >> 32) == cards.proxy &&
                 (single || uint32_t(last >> 32) == cards.proxy) &&
                 begin >= cards.begin && begin <= cards.end && end >= begin && end <= cards.end &&
                 (begin - cards.begin) % kStride == 0 && (end - cards.begin) % kStride == 0,
             Guard::Iterator)) {
    poisoned = true;
    Stop();
    return;
  }
  std::array<bool, kCells> cells{};
  for (size_t i = (begin - cards.begin) / kStride; i < (end - cards.begin) / kStride; ++i) {
    uint8_t cell = 0;
    if (!Cell(cards.At(i), cell)) { poisoned = true; Stop(); return; }
    cells[cell] = true;
  }
  Retire(cells, token);
}

}  // namespace

namespace brawlers::game {
void SetGateAudioCapture(ui::ReadBytes read_bytes, GateAudioOutput callbacks) {
  reader = std::move(read_bytes);
  output = callbacks;
  activation = GateAudioActivation::Disabled;
  scene_address = 0;
  identities.fill(0);
  cached = false;
  poisoned = false;
  resetting = 0;
  reported = 0;
}

void ConfigureGateAudioCapture(GateAudioActivation value) noexcept {
  Observe([&] {
    activation = value;
    Stop();
  });
}
}  // namespace brawlers::game

extern "C" void sub_82218AF0(PPCContext& ctx, uint8_t* base) {
  const auto scene = ctx.r3.u32;
  __imp__sub_82218AF0(ctx, base);
  Observe([&] {
    Stop();
    identities.fill(0);
    reported = 0;
    poisoned = !Check(!scene_address, Guard::DuplicateLifetime) ||
               !Check(scene && scene % 16 == 0 && scene <= UINT32_MAX - 0x24E2,
                      Guard::SceneAddress);
    scene_address = scene;
  });
}

extern "C" void sub_82216760(PPCContext& ctx, uint8_t* base) {
  const auto scene = ctx.r3.u32;
  Observe([&] {
    const auto token = Generation();
    if (activation != GateAudioActivation::NativeCardRendering) return;
    if (poisoned || resetting ||
        !Check(scene_address && scene_address == scene, Guard::MissingLifetime) ||
        !Capture(token)) Stop();
  });
  __imp__sub_82216760(ctx, base);
}

extern "C" void sub_82217C50(PPCContext& ctx, uint8_t* base) {
  const auto scene = ctx.r3.u32;
  Observe([&] {
    if (scene_address == scene) {
      Stop();
      identities.fill(0);
      scene_address = 0;
      poisoned = false;
    }
  });
  __imp__sub_82217C50(ctx, base);
}

extern "C" void sub_822180A8(PPCContext& ctx, uint8_t* base) {
  const auto scene = ctx.r3.u32;
  bool tracked = false;
  Observe([&] {
    if (scene_address == scene && scene) {
      tracked = true;
      ++resetting;
      Stop();
      identities.fill(0);
      poisoned = false;
    }
  });
  try {
    __imp__sub_822180A8(ctx, base);
  } catch (...) {
    Observe([&] {
      if (tracked && resetting) --resetting;
      if (tracked) { poisoned = true; Stop(); }
    });
    throw;
  }
  Observe([&] { if (tracked && resetting) --resetting; });
}

extern "C" void sub_82216F78(PPCContext& ctx, uint8_t* base) {
  const auto task = ctx.r3.u32;
  Observe([&] {
    if (!scene_address) return;
    uint32_t state = 0;
    if (!U32(uint64_t(task) + 0x18, state, Guard::TaskRead) || state == 1 || state == 2) Stop();
  });
  __imp__sub_82216F78(ctx, base);
}

extern "C" void sub_8226D8C0(PPCContext& ctx, uint8_t* base) {
  const auto container = ctx.r3.u32, source = ctx.r4.u32;
  Observe([&] { Insert(container, source, 1); });
  __imp__sub_8226D8C0(ctx, base);
}

extern "C" void sub_82222AC0(PPCContext& ctx, uint8_t* base) {
  const auto container = ctx.r3.u32, count = ctx.r5.u32, source = ctx.r6.u32;
  Observe([&] { Insert(container, source, count); });
  __imp__sub_82222AC0(ctx, base);
}

extern "C" void sub_82222638(PPCContext& ctx, uint8_t* base) {
  const auto container = ctx.r4.u32;
  const auto iterator = ctx.r5.u64;
  Observe([&] { Erase(container, iterator, 0, true); });
  __imp__sub_82222638(ctx, base);
}

extern "C" void sub_82222710(PPCContext& ctx, uint8_t* base) {
  const auto container = ctx.r4.u32;
  const auto first = ctx.r5.u64, last = ctx.r6.u64;
  Observe([&] { Erase(container, first, last, false); });
  __imp__sub_82222710(ctx, base);
}
