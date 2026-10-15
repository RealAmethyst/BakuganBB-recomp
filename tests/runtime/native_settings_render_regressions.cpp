// Synthetic guest memory and native-call boundaries; no game code or assets.
#include "../../src/game/native_settings_render.cpp"
#include <cassert>
#include <iostream>
#include <map>
std::map<uint32_t, uint8_t> bytes;
std::vector<std::string> diagnostics;
int frees = 0, animations = 0, geometry = 0, draws = 0, font_draws = 0;
int font_measures = 0, glyph_measures = 0;
int ordinary_font_calls = 0;
int title_callbacks = 0, hint_callbacks = 0;
uint32_t title_pointer = 0, hint_pointer = 0;
std::map<uint32_t, unsigned> callback_counts;
std::map<uint32_t, bool> callback_visibility;
float focus_zoom = 1, glow_pulse = 1, bounds_zoom = 1;
uint32_t focus_slot = 0;
void reserve(uint32_t a, size_t n) {
  for (size_t i = 0; i < n; ++i)
    bytes[a + i] = 0;
}
bool rd(uint32_t a, void *p, size_t n) {
  for (size_t i = 0; i < n; ++i) {
    auto f = bytes.find(a + i);
    if (f == bytes.end())
      return false;
    static_cast<uint8_t *>(p)[i] = f->second;
  }
  return true;
}
bool wr(uint32_t a, const void *p, size_t n) {
  for (size_t i = 0; i < n; ++i)
    if (!bytes.contains(a + i))
      return false;
  for (size_t i = 0; i < n; ++i)
    bytes[a + i] = static_cast<const uint8_t *>(p)[i];
  return true;
}
void word(uint32_t a, uint32_t v) {
  reserve(a, 4);
  uint8_t b[4];
  brawlers::settings::EncodeWord(b, v);
  assert(wr(a, b, 4));
}
void half(uint32_t a, uint16_t v) {
  reserve(a, 2);
  bytes[a] = v >> 8;
  bytes[a + 1] = v;
}
void flt(uint32_t a, float v) { word(a, std::bit_cast<uint32_t>(v)); }
bool QueueGameDiagnostic(std::string_view s) noexcept {
  diagnostics.emplace_back(s);
  return true;
}
extern "C" void sub_8212E940(PPCContext &c, uint8_t *) {
  reserve(0x10000, c.r3.u32);
  c.r3.u64 = 0x10000;
}
extern "C" void sub_8212EAD0(PPCContext &, uint8_t *) { ++frees; }
extern "C" void sub_823A8CE0(PPCContext &c, uint8_t *) { c.r3.u64 = 0x7000; }
extern "C" void __imp__sub_823C93F0(PPCContext &c, uint8_t *) {
  auto p = c.r5.u32;
  reserve(p, 96);
  if (p == brawlers::settings::state.storage + 0x3180) {
    assert(c.r3.u32 == 0x6000 && c.r4.u32 == 0x9000);
    assert(c.r6.u32 == 0xC000);
    assert(c.r8.u32 == 3 || c.r8.u32 == 13);
    assert(c.f1.f64 == 0);
    assert(brawlers::settings::memory.U32BE(c.r1.u32 + 0x54) == 0);
    for (uint32_t i = 0; i < 16; ++i)
      assert(*brawlers::settings::Float(c.r9.u32 + i * 4) == (i % 5 == 0 ? 1 : 0));
    for (uint32_t i = 0; i < 4; ++i)
      assert(*brawlers::settings::Float(c.r10.u32 + i * 4) == 1);
  }
  if (c.r8.u32 == 3) {
    // Synthetic type-one quad, including distinct non-origin corners.
    constexpr std::array<float, 8> corners{210, 6, 245, 33, 40, 39, 37, 9};
    for (size_t i = 0; i < corners.size(); ++i)
      flt(p + i * 4, i % 2 == 0 ? 141 + (corners[i] - 141) * bounds_zoom : corners[i]);
  } else if (c.r8.u32 == 13) {
    flt(p + 8, 1237);
    flt(p + 12, 449);
    flt(p + 24, 37);
    flt(p + 28, 9);
  } else {
    // 823D4700 emits the type-four right edge at these offsets. Equal X
    // coordinates are valid and must not be rejected as an empty sprite.
    flt(p + 8, 253);
    flt(p + 12, 155);
    flt(p + 24, 253);
    flt(p + 28, 31);
  }
}
void construct(PPCContext &c, uint32_t t) {
  auto a = c.r3.u32;
  word(a, t);
  bytes[a + 0xDA] = 1;
  word(a + 0xD0, 0x9000);
  word(a + 0xD4, 0xC000);
  half(a + 0xD8, brawlers::settings::memory.U32BE(c.r4.u32 + 0x1C).value_or(6));
  word(a + 0xF4, c.r5.u32);
  bytes[a + 0x100] = c.r5.u8;
  for (uint32_t i = 0; i < 16; ++i) flt(a + 0x30 + i * 4, i % 5 == 0 ? 1 : 0);
  flt(a + 0x60, 500);
  flt(a + 0x64, t == 0x82019DB4 ? 212 : 260);
}
extern "C" void __imp__sub_82336850(PPCContext &c, uint8_t *) {
  assert(c.r5.u32 < 5);
  assert(c.r4.u32 == brawlers::settings::state.storage);
  construct(c, 0x82019DA0);
}
extern "C" void __imp__sub_82336B00(PPCContext &c, uint8_t *) {
  assert(c.r5.u32 < 5);
  assert(c.r4.u32 == brawlers::settings::state.storage + 0x400);
  construct(c, 0x82019DB4);
}
extern "C" void __imp__sub_82337128(PPCContext &c, uint8_t *) {
  construct(c, 0x82019DD0);
}
extern "C" void __imp__sub_823375D8(PPCContext &c, uint8_t *) {
  construct(c, 0x82019DE4);
}
extern "C" void __imp__sub_82336E60(PPCContext &c, uint8_t *) {
  const auto sequence = brawlers::settings::memory.U32BE(c.r4.u32 + 0x1C);
  assert(sequence == 10 || sequence == 11);
  assert(c.r4.u32 == brawlers::settings::state.storage + (sequence == 10 ? 0x800 : 0xC00));
  construct(c, 0x82019DBC);
}
extern "C" void __imp__sub_821CDAC0(PPCContext &c, uint8_t *) {
  construct(c, 0x82019DF8);
}
extern "C" void __imp__sub_823C64A0(PPCContext &c, uint8_t *) {
  ++animations;
  auto a = c.r3.u32;
  ++callback_counts[a];
  for (int i = 0; i < 16; i++)
    flt(a + 0x70 + i * 4, *brawlers::settings::Float(a + 0x30 + i * 4));
  const auto found = brawlers::settings::state.widgets.find(a);
  if (found != brawlers::settings::state.widgets.end()) {
    const auto &item = found->second;
    const float zoom = item.slot == focus_slot ? focus_zoom : 1;
    const float pulse = item.kind == brawlers::settings::WidgetKind::Glow ? glow_pulse : 1;
    flt(a + 0x70, zoom * pulse);
    flt(a + 0x84, zoom * pulse);
  }
  PPCRegister r{};
  r.u64 = a;
  NativeSettingsRowTransform(r);
  callback_visibility[a] = bytes[a + 0xDA] != 0;
  if (bytes[a + 0xDA])
    ++geometry;
}
struct FontObservation {
  bool drawing;
  float scale, advance, width, height, origin_y;
  uint32_t lines;
};
std::vector<FontObservation> font_observations;
std::map<uint32_t, std::array<float, 16>> text_matrices;
std::map<uint32_t, brawlers::settings::TextBox> text_boxes;
float text_width(uint32_t pointer) {
  return pointer == brawlers::settings::TextAddress(brawlers::settings::kDescriptionText) ? 1200 : 500;
}
extern "C" void sub_823958B8(PPCContext &c, uint8_t *) {
  ++glyph_measures;
  flt(c.r5.u32, text_width(c.r4.u32));
  flt(c.r6.u32, 22);
}
extern "C" void __imp__sub_82396140(PPCContext &c, uint8_t *) {
  using namespace brawlers::settings;
  if (!font_fit.active) {
    ++ordinary_font_calls;
    assert(*Float(c.r3.u32 + 0x28) == 1 && *Float(c.r3.u32 + 0x2C) == 1);
    assert(memory.U32BE(c.r3.u32 + 0x30C) == 3);
    assert(memory.U32BE(c.r3.u32 + 0x310) == 1);
    assert(c.f4.f64 == 22);
    return;
  }
  if (c.r9.u8) ++font_draws; else ++font_measures;
  assert(memory.U32BE(0xA30C) == 0 && memory.U32BE(0xA310) == 0);
  const auto sx = *Float(c.r3.u32 + 0x28), sy = *Float(c.r3.u32 + 0x2C);
  // Original synthetic glyphs have uniform advances and a 22-unit height.
  // This boundary exposes the native metric contract; it is not a native
  // font implementation or evidence of the game's actual glyph widths.
  const auto lines = static_cast<uint32_t>(std::ceil(text_width(c.r4.u32) * sx / c.f3.f64));
  const float height = (lines - 1) * c.f4.f64 + 22 * sy;
  word(c.r3.u32 + 0x308, lines);
  flt(c.r3.u32 + 0x304, height);
  font_observations.push_back({c.r9.u8 != 0, sx, static_cast<float>(c.f4.f64),
                               static_cast<float>(c.f3.f64), height,
                               static_cast<float>(c.f2.f64), lines});
}
extern "C" void __imp__sub_823A9528(PPCContext &c, uint8_t *b) {
  using namespace brawlers::settings;
  ++draws;
  if (text_scope.kind == TextKind::Label) {
    text_boxes[c.r4.u32] = text_box;
    auto &matrix = text_matrices[c.r4.u32];
    for (size_t i = 0; i < matrix.size(); ++i) matrix[i] = *Float(c.r9.u32 + i * 4);
  }
  PPCContext bounds = c;
  bounds.r5.u64 = 0x18000;
  bounds.r8.u64 = 3;
  sub_823C93F0(bounds, b);
  const float width = std::abs(*Float(0x18008) - *Float(0x18018));
  const bool wide = text_box == TextBox::Wide;
  const float available = wide ? state.row_step * 0.85f : 24;
  const auto first = font_observations.size();
  const auto before_glyphs = glyph_measures;
  PPCContext font = c;
  font.r3.u64 = 0xA000; font.r9.u64 = 0;
  font.f2.f64 = 0; font.f3.f64 = width; font.f4.f64 = 22;
  sub_82396140(font, b);
  assert(!state.fault);
  const auto height = *Float(0xA304);
  assert(height > 0 && height <= available + 0.001f);
  const auto &measured = font_observations.back();
  assert(!measured.drawing);
  if (wide) {
    // A short two-line wide caption keeps its original font size. A long
    // caption shrinks for height, rather than collapsing into one tiny line.
    if (text_width(c.r4.u32) == 500) assert(measured.scale == 1);
    else assert(measured.scale > 0.6f && measured.scale < 1 && measured.lines >= 2);
    assert(font_observations.size() == first + 2);
    assert(font_observations[first].scale == 1);
    assert(font_observations[first].lines >= 2);
    assert(glyph_measures == before_glyphs);
  } else {
    assert(font_observations.size() == first + 1);
    assert(measured.lines == 1 && measured.scale < 1);
    assert(glyph_measures == before_glyphs + 1);
  }
  const auto measurement = measured;
  assert(*Float(0xA028) == 1 && *Float(0xA02C) == 1);
  font = c;
  font.r3.u64 = 0xA000; font.r9.u64 = 1;
  font.f2.f64 = (available - height) / 2;
  font.f3.f64 = width; font.f4.f64 = 22;
  sub_82396140(font, b);
  const auto &drawn = font_observations.back();
  assert(drawn.drawing && drawn.lines == measurement.lines);
  assert(std::abs(drawn.scale - measurement.scale) < 0.0001f);
  assert(std::abs(drawn.advance - measurement.advance) < 0.0001f);
  assert(std::abs(drawn.height - height) < 0.0001f);
  assert(std::abs(drawn.origin_y - (available - height) / 2) < 0.0001f);
  assert(*Float(0xA028) == 1 && *Float(0xA02C) == 1);
  assert(memory.U32BE(0xA30C) == 3 && memory.U32BE(0xA310) == 1);
}
extern "C" void __imp__sub_82336C28(PPCContext &c, uint8_t *b) {
  auto a = c.r3.u32;
  PPCContext provider{};
  provider.r3.u64 = 0xDEAD;
  const auto slot = bytes[a + 0x100];
  brawlers::settings::NativeSettingsRenderProvider(provider, 0, brawlers::settings::kLabelIds[slot], 0);
  assert(provider.r3.u32 == brawlers::settings::TextAddress(
      brawlers::settings::Present(slot) ? slot * 2 : brawlers::settings::kEmptyText));
  sub_823C64A0(c, b);
  c.r4.u64 = provider.r3.u32;
  c.r9.u64 = a + 0x70;
  assert(brawlers::settings::NativeSettingsRenderText(c, b));
}
extern "C" void __imp__sub_821E9908(PPCContext &, uint8_t *) {
  ++title_callbacks;
  PPCContext provider{};
  provider.r3.u64 = 0xDEAD;
  brawlers::settings::NativeSettingsRenderProvider(provider, 0, 0x612, 0);
  title_pointer = provider.r3.u32;
}
extern "C" void __imp__sub_821EA650(PPCContext &c, uint8_t *b) {
  ++hint_callbacks;
  PPCContext provider{};
  provider.r3.u64 = 0xDEAD;
  brawlers::settings::NativeSettingsRenderProvider(provider, 0, 0xA21, 0);
  hint_pointer = provider.r3.u32;
  c.r4.u64 = hint_pointer;
  assert(brawlers::settings::NativeSettingsRenderText(c, b));
}
uint32_t layout_sequence(uint32_t id, uint32_t tracks) {
  const auto entry = 0x30000 + id * 0x100;
  reserve(entry, 0x100);
  word(0xD008 + id * 4, entry);
  word(entry + 4, tracks + 1);
  for (uint32_t track = 0; track < tracks; ++track) {
    const auto storage = 0x40000 + id * 0x1000 + track * 0x100;
    reserve(storage, 0x100);
    word(entry + 16 + track * 4, storage);
  }
  return entry;
}
uint32_t layout_record(uint32_t sequence, uint32_t track, uint32_t index,
                       uint8_t type, uint16_t frame, uint16_t child,
                       int16_t x, int16_t y, int16_t sx = 1024, int16_t sy = 1024) {
  const auto storage = 0x40000 + sequence * 0x1000 + track * 0x100;
  const auto size = type == 3 ? 0x34 : 0x48;
  const auto record = storage + 4 + index * size;
  half(storage, index + 1);
  bytes[record + 1] = size / 4;
  half(record + 2, frame);
  bytes[record + 4] = type;
  half(record + 6, child);
  half(record + 0x10, x); half(record + 0x12, y);
  half(record + (type == 3 ? 0x30 : 0x34), sx);
  half(record + (type == 3 ? 0x32 : 0x36), sy);
  return record;
}
void layout_resources() {
  word(0xD000, 17);
  layout_sequence(13, 1);
  const auto body = 0x40000 + 13 * 0x1000 + 4;
  half(body - 4, 1); bytes[body + 1] = 0x44 / 4; bytes[body + 4] = 1;
  layout_sequence(16, 1);
  layout_record(16, 0, 0, 3, 3, 15, 610, 295);
  layout_record(16, 0, 1, 3, 0, 15, 630, 310);
  layout_sequence(15, 1);
  layout_record(15, 0, 0, 3, 0, 14, -130, -50);
  layout_sequence(14, 5);
  layout_record(14, 1, 0, 3, 0, 3, 0, -48);
  layout_record(14, 4, 0, 3, 0, 10, 0, 0);
  layout_record(14, 3, 0, 3, 2, 11, 0, 0, 1216, 1216);
  layout_record(14, 3, 1, 3, 0, 11, 0, 0);
  layout_sequence(8, 1);
  layout_record(8, 0, 0, 3, 2, 14, 0, 0, 1152, 1152);
  layout_record(8, 0, 1, 3, 0, 14, 0, 0);
  // Original synthetic three-piece panels: 200x150 tiles, with asymmetric
  // corner warps. Union [-308,-78]..[312,80] is substantially taller than a row.
  half(0xD508, 0);
  flt(0xD50C, 0); flt(0xD510, 0);
  flt(0xD514, 150.0f / 256); flt(0xD518, 200.0f / 512);
  for (uint32_t id : {10u, 11u}) {
    layout_sequence(id, 3);
    for (uint32_t track = 0; track < 3; ++track) {
      const auto record = layout_record(id, track, 0, 4, 0, 0,
                                        static_cast<int16_t>(-300 + 200 * static_cast<int>(track)), -75);
      if (track == 0) {
        half(record + 0x2C, -8); half(record + 0x2E, 4);
        half(record + 0x30, -8); half(record + 0x32, -3);
      }
      if (track == 2) {
        half(record + 0x24, 12); half(record + 0x26, -2);
        half(record + 0x28, 12); half(record + 0x2A, 5);
      }
    }
  }
}
int main() {
  using namespace brawlers::settings;
  SetNativeSettingsRenderAccess(rd, wr);
  reserve(0x1000, 0x100);
  reserve(0x2000, 0x100);
  reserve(0x4000, 32);
  reserve(0x5000, 12);
  reserve(0x6000, 0x130);
  reserve(0x7000, 16);
  reserve(0x8000, 0x60);
  reserve(0x9000, 32);
  reserve(0xC000, 12);
  reserve(0xC100, 20);
  reserve(0xD000, 0x800);
  // Relocated resource structures, with independent synthetic keyframes.
  word(0x9008, 0xD000);
  word(0x900C, 0xD500);
  word(0xD000, 7);
  word(0xD008 + 3 * 4, 0xD100);
  word(0xD008 + 6 * 4, 0xD140);
  word(0xD104, 2);
  word(0xD110, 0xD200);
  word(0xD144, 2);
  word(0xD150, 0xD300);
  half(0xD200, 1);
  bytes[0xD205] = 0x44 / 4;
  bytes[0xD208] = 1;
  half(0xD300, 2);
  bytes[0xD305] = 0x48 / 4;
  half(0xD306, 2);
  bytes[0xD308] = 4;
  half(0xD30A, 1);
  bytes[0xD34D] = 0x48 / 4;
  bytes[0xD350] = 4;
  half(0xD352, 1);
  word(0xD500, 2);
  half(0xD51C, 0);
  flt(0xD520, 0.1f);
  flt(0xD524, 0.2f);
  flt(0xD528, 0.4f);
  flt(0xD52C, 0.6f);
  word(0xC004, 0xC100);
  word(0xC008, 1);
  flt(0xC104, 512);
  flt(0xC108, 256);
  assert(BoundsContract(0x9000, 0xC000, 3, 1));
  assert(BoundsContract(0x9000, 0xC000, 6, 4));
  layout_resources();
  bytes[0xD308] = 1; // The preceding keyframe is consumed too.
  assert(!BoundsContract(0x9000, 0xC000, 6, 4));
  assert(diagnostics.back() == "settings_render_bounds_record_type");
  state.fault = false;
  bytes[0xD308] = 4;
  half(0xD352, 2);
  assert(!BoundsContract(0x9000, 0xC000, 6, 4));
  assert(diagnostics.back() == "settings_render_bounds_uv_count");
  state.fault = false;
  half(0xD352, 1);
  half(0xD51C, 1);
  assert(!BoundsContract(0x9000, 0xC000, 6, 4));
  assert(diagnostics.back() == "settings_render_bounds_texture_table");
  state.fault = false;
  half(0xD51C, 0);
  bytes[0xD359] = 2;
  assert(!BoundsContract(0x9000, 0xC000, 6, 4));
  assert(diagnostics.back() == "settings_render_bounds_interpolation_tail");
  state.fault = false;
  bytes[0xD359] = 0;
  assert(BoundsContract(0x9000, 0xC000, 6, 4));
  reserve(0xA000, 0x318);
  reserve(0xB000, 0x58);
  word(0x829CD808, 0xA000);
  word(0xA00C, 0xB000);
  flt(0xA028, 1);
  flt(0xA02C, 1);
  word(0xA30C, 3);
  word(0xA310, 1);
  reserve(0xE000, 0x1000);
  word(0x1000, 0x82019C40);
  // Native 82334FC0 initializes model+0 to the numeric context value 2.
  // The shared owner is separately published at 829CBC68, not in that field.
  word(0x1020, 2);
  word(0x2000, 0x82003EAC);
  word(0x829CBC68, 0x2000);
  word(0x829CDA60, 0x7000);
  word(0x7004, 0x6000);
  word(0x60CC, 0x8000);
  half(0x805C, 1280);
  half(0x805E, 720);
  word(0x400C, 0x5000);
  word(0x4010, 0x500C);
  word(0x4018, 1);
  word(0x401C, 6);
  PPCContext c{};
  c.r1.u64 = 0xE000;
  // Even a readable decoy in model+0 cannot substitute for the shared owner.
  word(0x1020, 0x2000);
  word(0x829CBC68, 0);
  assert(!NativeSettingsRenderBegin(0x1020, c, nullptr));
  word(0x829CBC68, 0x2000);
  word(0x2000, 0);
  assert(!NativeSettingsRenderBegin(0x1020, c, nullptr));
  word(0x2000, 0x82003EAC);
  word(0x1020, 2);
  assert(NativeSettingsRenderBegin(0x1020, c, nullptr));
  uint32_t next = 0x20000;
  auto call = [&](auto fn, uint32_t row, uint32_t r6 = 0, uint32_t sequence = 6) {
    reserve(next, 0x200);
    word(0x401C, sequence);
    c.r3.u64 = next;
    c.r4.u64 = 0x4000;
    c.r5.u64 = row;
    c.r6.u64 = r6;
    fn(c, nullptr);
    next += 0x200;
  };
  for (uint32_t i = 0; i < 5; i++) {
    call(sub_82336850, i);
    if (i < 2) {
      call(sub_82337128, 0, i);
      call(sub_82337128, 1, i);
    } else
      call(sub_823375D8, i);
    call(sub_82336B00, i, 0, 3);
    call(sub_82336E60, i, 0, 10);
    call(sub_82336E60, i, 0, 11);
  }
  call(sub_821CDAC0, 0);
  call(sub_821CDAC0, 4);
  assert(NativeSettingsRenderBuilt(0x1020, c, nullptr));
  assert(state.widgets.size() == 29);
  for (uint32_t i = 0; i < 4; ++i) {
    const auto address = state.storage + i * 0x400;
    assert(memory.U32BE(address + 0x1C) == std::array{6u, 3u, 10u, 11u}[i]);
    assert(memory.U32BE(address + 0xC) == address + 32);
    assert(memory.U32BE(address + 0x10) == address + 44);
  }
  for (uint32_t i = 0; i < 5; ++i) {
    assert(memory.U32BE(state.selections[i] + 0xF4) == i);
    assert(memory.U32BE(state.labels[i] + 0xF4) == i);
    for (uint32_t kind = 0; kind < 2; ++kind)
      assert(memory.U32BE(state.panels[kind][i] + 0xF4) == i);
  }
  for (const auto &bounds : state.panel_bounds) {
    assert(bounds.left == -308 && bounds.top == -78);
    assert(bounds.right == 312 && bounds.bottom == 80);
  }
  assert(state.label_width == 208 && state.label_height == 24);
  // Reserve the complete animated envelope rather than hiding an overlarge
  // selection sprite behind a tiny synthetic height or an oversized viewport.
  const auto zoom_scale = 0x40000 + 8 * 0x1000 + 4 + 0x32;
  half(zoom_scale, 8192);
  assert(!MeasureLayout(c, nullptr));
  assert(diagnostics.back() == "settings_render_layout_does_not_fit");
  half(zoom_scale, 1152);
  state.fault = false;
  assert(MeasureLayout(c, nullptr));
  View v;
  v.open = true;
  v.title = "Options";
  v.section = "Audio";
  v.hints = "A select B back";
  v.page = 1;
  for (int i = 0; i < 9; i++)
    v.rows.push_back({static_cast<Id>(i + 1), RowKind::Range, "Cue", "100%",
                      "Description", 100});
  v.position = 9;
  assert(NativeSettingsRenderPublish(v));
  assert(NativeSettingsRenderSelection() == 4);
  assert(state.first == 4);
  assert(state.visible == 5);
  v.position = 1;
  assert(NativeSettingsRenderPublish(v));
  assert(state.first == 0);
  assert(NativeSettingsRenderSelection() == 0);
  v.rows[0].label = "Unpublished change";
  assert(state.view.rows[0].label == "Cue");
  v.rows[0].label = "Cue";
  c.r3.u64 = state.labels[0];
  int before = animations;
  sub_82336C28(c, nullptr);
  assert(animations == before + 1);
  assert(draws == 4);
  assert(font_draws == 4);
  assert(font_measures == 6);
  assert(bytes[state.labels[0] + 0xDA] == 1);
  assert(state.fault == false);
  // The owned hint keeps the native box and fits its complete caption into
  // one authored line. Verify the font state is restored for later widgets.
  {
    reserve(0x70000, 0x100); reserve(0x70100, 0x100);
    reserve(0x70200, 0x20); reserve(0x70300, 8); reserve(0x70400, 0x100);
    word(0x7010C, 0x70200); word(0x7020C, 0x70300); word(0x70210, 0x70308);
    word(0x70300, 0x204C); word(0x70304, 0x2094);
    word(0x70000, 0x82003EBC); half(0x7000A, 0);
    half(0x7040A, 1); word(0x2058, 9); word(0x20C4, 0xA21);
    PPCContext title = c;
    title.r3.u64 = 0x70000; title.r4.u64 = 0x70100;
    sub_821E9908(title, nullptr);
    assert(title_callbacks == 1 && title_pointer == TextAddress(kTitleText));
    word(0x70300, 0x2050);
    sub_821E9908(title, nullptr);
    assert(title_callbacks == 2 && title_pointer == 0xDEAD);
    word(0x70300, 0x204C);
    PPCContext hint = c;
    hint.r3.u64 = 0x70400; hint.r4.u64 = 0x70100;
    assert(NativeSettingsRenderHints(hint, nullptr));
    assert(hint_callbacks == 1 && hint_pointer == TextAddress(kHintsText));
    assert(font_draws == 5);
    assert(font_measures == 7);
    assert(!state.fault);
    word(0x70304, 0x2098);
    assert(!NativeSettingsRenderHints(hint, nullptr));
    word(0x70304, 0x2094); word(0x20C4, 0xA22);
    assert(!NativeSettingsRenderHints(hint, nullptr));
    assert(hint_callbacks == 1);
    word(0x20C4, 0xA21);
  }
  PPCContext ordinary = c;
  ordinary.r3.u64 = 0xA000;
  ordinary.r9.u64 = 1;
  ordinary.f3.f64 = 100;
  ordinary.f4.f64 = 22;
  sub_82396140(ordinary, nullptr);
  assert(ordinary_font_calls == 1);
  // Empty native text returns before resetting native metrics. The owned
  // wrapper must clear them without calling either measurement boundary.
  {
    ScopedText scope(TextKind::Label, 0);
    ScopedFontFit fit;
    text_box = TextBox::Wide;
    flt(0xA304, 999); word(0xA308, 99);
    const auto calls = font_observations.size();
    const auto raw_calls = glyph_measures;
    PPCContext empty = ordinary;
    empty.r4.u64 = TextAddress(kEmptyText); empty.r9.u64 = 0;
    sub_82396140(empty, nullptr);
    assert(empty.r3.u32 == 0 && *Float(0xA304) == 0 && memory.U32BE(0xA308) == 0);
    assert(font_observations.size() == calls && glyph_measures == raw_calls);
    assert(*Float(0xA028) == 1 && memory.U32BE(0xA30C) == 3);
    empty.r3.u64 = 0xA000; empty.r9.u64 = 1;
    sub_82396140(empty, nullptr);
    assert(empty.r3.u32 == 0 && font_observations.size() == calls);
    text_box = TextBox::Native;
  }
  // A category/action caption fills the button, while a setting's label and
  // value retain separate columns. Native centering consumes these bounds.
  assert(text_boxes.at(TextAddress(0)) == TextBox::Row);
  for (auto kind : {RowKind::Page, RowKind::Reset}) {
    v.rows[0].kind = kind; v.rows[0].value.clear();
    assert(NativeSettingsRenderPublish(v));
    c.r3.u64 = state.labels[0]; sub_82336C28(c, nullptr);
    assert(text_boxes.at(TextAddress(0)) == TextBox::Button);
  }
  v.rows[0].kind = RowKind::Range; v.rows[0].value = "100%";
  assert(NativeSettingsRenderPublish(v));
  {
    ScopedText scope(TextKind::Label, 0);
    PPCContext bounds = c;
    bounds.r5.u64 = 0x18000; bounds.r8.u64 = 3;
    for (float zoom : {1.0f, 1.125f}) {
      bounds_zoom = zoom;
      for (auto box : {TextBox::Row, TextBox::Button, TextBox::Wide}) {
        text_box = box;
        sub_823C93F0(bounds, nullptr);
        const auto left = *Float(0x18018), right = *Float(0x18008);
        const bool full_button = box != TextBox::Row;
        const auto expected_center = 141 + (full_button ? 114.4f : 0);
        const auto expected_width = (full_button ? 436.8f : 208.0f) * zoom;
        assert(std::abs((left + right) * 0.5f - expected_center) < 0.001f);
        assert(std::abs(right - left - expected_width) < 0.001f);
        assert(*Float(0x1800C) == 33 && *Float(0x1801C) == 9);
      }
    }
    bounds_zoom = 1; text_box = TextBox::Native;
  }
  std::array<uint8_t, 0x1000> templates_before{};
  std::array<uint8_t, 0x1E00> text_before{};
  assert(rd(state.storage, templates_before.data(), templates_before.size()));
  assert(rd(state.storage + 0x1000, text_before.data(), text_before.size()));
  auto frame = [&](int expected_geometry = 15) {
    const auto counts = callback_counts;
    const auto previous_animations = animations;
    const auto previous_geometry = geometry;
    for (const auto &[address, item] : state.widgets) {
      PPCContext draw = c;
      draw.r3.u64 = address;
      if (item.kind == WidgetKind::Label) sub_82336C28(draw, nullptr);
      else sub_823C64A0(draw, nullptr);
      const auto prior = counts.contains(address) ? counts.at(address) : 0;
      assert(callback_counts[address] == prior + 1);
      assert(bytes[address + 0xDA] == 1); // Native draw visibility is scoped.
      if (item.kind == WidgetKind::Hidden || item.kind == WidgetKind::Selection || state.suspended)
        assert(!callback_visibility.at(address));
      if (state.view.confirming && (item.slot >= 3 ||
          (item.slot == 0 && item.kind != WidgetKind::Label)))
        assert(!callback_visibility.at(address));
    }
    assert(animations == previous_animations + 29);
    assert(geometry == previous_geometry + (state.suspended ? 0 : expected_geometry));
  };
  std::optional<std::array<float, 16>> header, footer;
  for (focus_slot = 0; focus_slot < 5; ++focus_slot) {
    for (float zoom : {1.0f, 1.125f}) for (float pulse : {1.0f, 1.1875f}) {
      focus_zoom = zoom; glow_pulse = pulse;
      frame();
      std::array<float, 5> row_top, row_bottom;
      row_top.fill(720); row_bottom.fill(0);
      for (uint32_t slot = 0; slot < 5; ++slot) for (uint32_t kind = 0; kind < 2; ++kind) {
        const auto widget = state.panels[kind][slot];
        const auto &bounds = state.panel_bounds[kind];
        for (float x : {bounds.left, bounds.right}) for (float y : {bounds.top, bounds.bottom}) {
          const auto px = x * *Float(widget + 0x70) + y * *Float(widget + 0x80) + *Float(widget + 0xA0);
          const auto py = x * *Float(widget + 0x74) + y * *Float(widget + 0x84) + *Float(widget + 0xA4);
          assert(px >= 0 && px <= 1280 && py >= 0 && py <= 720);
          row_top[slot] = std::min(row_top[slot], py);
          row_bottom[slot] = std::max(row_bottom[slot], py);
        }
      }
      for (uint32_t slot = 1; slot < 5; ++slot) assert(row_top[slot] > row_bottom[slot - 1]);
      const auto &h = text_matrices.at(TextAddress(kSectionText));
      const auto &f = text_matrices.at(TextAddress(kDescriptionText));
      if (!header) { header = h; footer = f; }
      assert(h == *header && f == *footer);
      assert(h[0] == 1 && h[5] == 1 && f[0] == 1 && f[5] == 1);
      assert(std::abs(h[13] - (212 - state.row_step)) < 0.001f);
      assert(std::abs(f[13] - (212 + 5 * state.row_step)) < 0.001f);
    }
  }
  std::array<uint8_t, 0x1000> templates_after{};
  std::array<uint8_t, 0x1E00> text_after{};
  assert(rd(state.storage, templates_after.data(), templates_after.size()));
  assert(rd(state.storage + 0x1000, text_after.data(), text_after.size()));
  assert(templates_before == templates_after && text_before == text_after);
  NativeSettingsRenderSuspend(true);
  frame();
  c.r3.u64 = state.selections[0];
  before = geometry;
  sub_823C64A0(c, nullptr);
  assert(geometry == before);
  assert(bytes[state.selections[0] + 0xDA] == 1);
  NativeSettingsRenderSuspend(false);
  v.confirming = true;
  v.confirm_yes = false;
  v.confirmation = "Reset this category?";
  v.yes = "Yes";
  v.no = "No";
  v.notice = "Save failed";
  assert(NativeSettingsRenderPublish(v));
  assert(NativeSettingsRenderSelection() == 2);
  assert(state.visible == 3);
  frame(7);
  assert(text_boxes.at(TextAddress(0)) == TextBox::Wide);
  assert(text_boxes.at(TextAddress(2)) == TextBox::Button);
  assert(text_boxes.at(TextAddress(4)) == TextBox::Button);
  c.r3.u64 = state.selections[0];
  before = geometry;
  sub_823C64A0(c, nullptr);
  assert(geometry == before);
  c.r3.u64 = state.panels[0][2];
  sub_823C64A0(c, nullptr);
  assert(geometry == before + 1);
  const auto old = state.view.title;
  v.title = std::string(300, 'x');
  assert(!NativeSettingsRenderPublish(v));
  assert(state.view.title == old);
  assert(!NativeSettingsRenderHealthy());
  NativeSettingsRenderEnd(0x1020, c, nullptr);
  assert(frees == 1);
  assert(!state.model);
  std::cout << "PASS: relocated bounds guards, native-sized asymmetric panel quads, "
               "four independent templates, logical slots, five-row animation envelope "
               "within 1280x720, oversized-envelope rejection, stable header/footer, "
               "all 29 callbacks including hidden originals, visibility restoration, "
               "suspension and confirmation, title/hints context and provider paths, "
               "wrapped Wide and one-line Row/Hints fitting, identical measured/drawn "
               "metrics and alignment, empty-text reset, font state restoration, "
               "storage separation, ordinary passthrough, and teardown\n";
}
