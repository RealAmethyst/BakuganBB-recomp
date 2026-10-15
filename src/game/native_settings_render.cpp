#include "native_settings_render.h"

#include "native_game_ui.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <map>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <rex/ppc/context.h>

#define ORIGINAL(address) extern "C" void __imp__sub_##address(PPCContext&, uint8_t*)
ORIGINAL(82336850);
ORIGINAL(82336B00);
ORIGINAL(82337128);
ORIGINAL(823375D8);
ORIGINAL(82336E60);
ORIGINAL(821CDAC0);
ORIGINAL(82336C28);
ORIGINAL(821E9908);
ORIGINAL(821EA650);
ORIGINAL(823C64A0);
ORIGINAL(823A9528);
ORIGINAL(823C93F0);
ORIGINAL(82396140);
#undef ORIGINAL
extern "C" void sub_8212E940(PPCContext&, uint8_t*);
extern "C" void sub_8212EAD0(PPCContext&, uint8_t*);
extern "C" void sub_823A8CE0(PPCContext&, uint8_t*);
extern "C" void sub_823C93F0(PPCContext&, uint8_t*);
extern "C" void sub_823958B8(PPCContext&, uint8_t*);

namespace brawlers::settings {
namespace {
constexpr uint32_t kSlots = 5;
constexpr uint32_t kStorageSize = 0x4000;
constexpr uint32_t kTemplateSize = 0x400;
constexpr uint32_t kTextSize = 0x200;
constexpr uint32_t kTextStart = 0x1000;
constexpr uint32_t kMatrix = 0x3000;
constexpr uint32_t kTitleText = 10;
constexpr uint32_t kHintsText = 11;
constexpr uint32_t kDescriptionText = 12;
constexpr uint32_t kEmptyText = 13;
constexpr uint32_t kSectionText = 14;
constexpr std::array<uint32_t, kSlots> kLabelIds{0x928, 0xA0D, 0x7BE, 0x7BF, 0x7C0};

enum class WidgetKind { Selection, Label, Panel, Glow, Hidden };
struct Rect {
  float left = 0, top = 0, right = 0, bottom = 0;
  float width() const { return right - left; }
  float height() const { return bottom - top; }
};
struct Widget {
  WidgetKind kind;
  uint32_t slot;
  uint32_t table;
};
enum class TextKind { None, Label, Title, Hints };
struct TextScope {
  TextKind kind = TextKind::None;
  uint32_t slot = 0;
};
struct State {
  uint32_t model = 0, common = 0, storage = 0;
  bool building = false, built = false, fault = false, suspended = false;
  std::array<bool, 4> templates{};
  std::array<uint32_t, kSlots> labels{}, selections{};
  std::array<std::array<uint32_t, kSlots>, 2> panels{};
  uint32_t values = 0, pairs = 0, actions = 0;
  std::map<uint32_t, Widget> widgets;
  View view;
  size_t first = 0;
  uint32_t visible = 0, selected = 0;
  float row_step = 0, value_offset = 0, label_width = 0, label_height = 0;
  std::array<Rect, 2> panel_bounds{};
  std::array<float, 2> panel_scale_x{}, panel_scale_y{}, panel_offset_x{}, panel_offset_y{};
};
ui::ReadBytes read;
std::function<bool(uint32_t, const void*, size_t)> write;
ui::GuestUiMemory memory({});
State state;
thread_local TextScope text_scope;
enum class TextBox { Native, Row, Button, Wide };
thread_local TextBox text_box = TextBox::Native;
struct FontFit {
  bool active = false;
  std::optional<double> factor;
};
thread_local FontFit font_fit;

struct ScopedFontFit {
  FontFit previous = font_fit;
  ScopedFontFit() { font_fit = {true, {}}; }
  ~ScopedFontFit() { font_fit = previous; }
};

struct ScopedText {
  TextScope previous = text_scope;
  explicit ScopedText(TextKind kind, uint32_t slot = 0) { text_scope = {kind, slot}; }
  ~ScopedText() { text_scope = previous; }
};

bool Fail(std::string_view reason) {
  if (!state.fault) QueueGameDiagnostic(reason);
  state.fault = true;
  return false;
}

bool Range(uint32_t address, size_t size) {
  if (!address || !size || size > UINT32_MAX || address > UINT32_MAX - size) return false;
  std::array<uint8_t, 1024> bytes{};
  try {
    for (size_t offset = 0; offset < size; offset += bytes.size()) {
      const auto count = (std::min)(bytes.size(), size - offset);
      if (!read || !read(address + static_cast<uint32_t>(offset), bytes.data(), count)) return false;
    }
    return true;
  } catch (...) { return false; }
}

bool Write(uint32_t address, const void* bytes, size_t size) {
  try {
    return (write && write(address, bytes, size)) || Fail("settings_render_write");
  } catch (...) { return Fail("settings_render_write_exception"); }
}

void EncodeWord(uint8_t* bytes, uint32_t value) {
  bytes[0] = static_cast<uint8_t>(value >> 24);
  bytes[1] = static_cast<uint8_t>(value >> 16);
  bytes[2] = static_cast<uint8_t>(value >> 8);
  bytes[3] = static_cast<uint8_t>(value);
}

bool Word(uint32_t address, uint32_t value) {
  std::array<uint8_t, 4> bytes{};
  EncodeWord(bytes.data(), value);
  return Write(address, bytes.data(), bytes.size());
}

std::optional<float> Float(uint32_t address) {
  const auto word = memory.U32BE(address);
  if (!word) return {};
  const auto value = std::bit_cast<float>(*word);
  return std::isfinite(value) ? std::optional(value) : std::nullopt;
}

bool AddFloat(uint32_t address, float delta) {
  const auto value = Float(address);
  return value ? Word(address, std::bit_cast<uint32_t>(*value + delta))
               : Fail("settings_render_transform_float");
}

uint32_t TextAddress(uint32_t slot) { return state.storage + kTextStart + slot * kTextSize; }

// Native expansion recognizes F000..F109 as substitutions. Owned captions are
// plain Unicode; refusing that range keeps expansion bounded to the input.
bool EncodeText(std::string_view text, std::array<uint8_t, kTextSize>& bytes) {
  size_t output = 0;
  auto append = [&](uint16_t unit) {
    if (output + 2 >= bytes.size()) return false;
    bytes[output++] = static_cast<uint8_t>(unit >> 8);
    bytes[output++] = static_cast<uint8_t>(unit);
    return true;
  };
  for (size_t i = 0; i < text.size();) {
    uint32_t cp = static_cast<uint8_t>(text[i++]);
    size_t extra = 0;
    uint32_t minimum = 0;
    if (cp >= 0xF0 && cp <= 0xF4) { cp &= 7; extra = 3; minimum = 0x10000; }
    else if (cp >= 0xE0 && cp <= 0xEF) { cp &= 15; extra = 2; minimum = 0x800; }
    else if (cp >= 0xC2 && cp <= 0xDF) { cp &= 31; extra = 1; minimum = 0x80; }
    else if (cp >= 0x80) return false;
    if (extra > text.size() - i) return false;
    while (extra--) {
      const auto next = static_cast<uint8_t>(text[i++]);
      if ((next & 0xC0) != 0x80) return false;
      cp = (cp << 6) | (next & 63);
    }
    if (!cp || cp < minimum || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF) ||
        (cp >= 0xF000 && cp <= 0xF1FF)) return false;
    if (cp < 0x10000) {
      if (!append(static_cast<uint16_t>(cp))) return false;
    } else {
      cp -= 0x10000;
      if (!append(static_cast<uint16_t>(0xD800 + (cp >> 10))) ||
          !append(static_cast<uint16_t>(0xDC00 + (cp & 1023)))) return false;
    }
  }
  return true;
}

bool Present(uint32_t slot) {
  return state.built && !state.fault && !state.suspended && state.view.open && slot < state.visible;
}

bool Identity(uint32_t address, const Widget& widget) {
  return memory.U32BE(address) == widget.table &&
      (widget.kind != WidgetKind::Label || memory.U8(address + 0x100) == widget.slot) &&
      ((widget.kind != WidgetKind::Selection && widget.kind != WidgetKind::Panel &&
        widget.kind != WidgetKind::Glow) || memory.U32BE(address + 0xF4) == widget.slot);
}

// Match the exact context-vector consumer used by the native row/header/hints
// callbacks, rather than treating a readable pointer as a record identity.
std::optional<uint32_t> Record(uint32_t widget, uint32_t context) {
  const auto vector = memory.U32BE(context + 0xC);
  const auto index = memory.U16BE(widget + 0xA);
  if (!vector || !*vector || !index) return {};
  const auto begin = memory.U32BE(*vector + 0xC), end = memory.U32BE(*vector + 0x10);
  if (!begin || !end || *end < *begin || (*end - *begin) % 4 ||
      (*end - *begin) / 4 <= *index || !Range(*begin, *end - *begin)) return {};
  return memory.U32BE(*begin + *index * 4);
}

bool Template(uint32_t source, uint32_t which) {
  if (state.templates[which]) return true;
  const auto begin = memory.U32BE(source + 0xC), end = memory.U32BE(source + 0x10);
  if (!begin || !end || *end < *begin || (*end - *begin) % 12 ||
      *end - *begin > kTemplateSize - 32 || !Range(source, 32) ||
      (*end != *begin && !Range(*begin, *end - *begin))) return Fail("settings_render_template_range");
  std::array<uint8_t, kTemplateSize> bytes{};
  if (!read(source, bytes.data(), 32) ||
      (*end != *begin && !read(*begin, bytes.data() + 32, *end - *begin)))
    return Fail("settings_render_template_copy");
  // These resource paths must create per-widget callbacks. A nonzero borrowed
  // callback could instead belong to the original row; do not duplicate it.
  for (uint32_t offset = 0; offset < *end - *begin; offset += 12)
    if (memory.U32BE(*begin + offset + 8) != 0) return Fail("settings_render_template_borrowed_callback");
  const auto destination = state.storage + which * kTemplateSize;
  EncodeWord(bytes.data() + 0xC, destination + 32);
  EncodeWord(bytes.data() + 0x10, destination + 32 + *end - *begin);
  EncodeWord(bytes.data() + 0x14, destination + 32 + *end - *begin);
  if (!Write(destination, bytes.data(), bytes.size())) return false;
  state.templates[which] = true;
  return true;
}

// Validate the relocated sequence/track chain consumed by 823C8A60. Its
// frame-zero dispatch reads both the selected and preceding keyframe, even
// when interpolation is disabled. Type four additionally consumes UV and
// texture descriptors; the auxiliary object is not a sequence/part number.
bool BoundsContract(uint32_t resource, uint32_t auxiliary, uint32_t sequence, uint8_t type) {
  const auto table = memory.U32BE(resource + 8);
  if (!table || !Range(*table, 8)) return Fail("settings_render_bounds_sequence_table");
  const auto count = memory.U32BE(*table);
  if (!count || !*count || *count > 1024 || sequence >= *count ||
      !Range(*table, 8 + *count * 4)) return Fail("settings_render_bounds_sequence_count");
  const auto entry = memory.U32BE(*table + 8 + sequence * 4);
  if (!entry || !Range(*entry, 20)) return Fail("settings_render_bounds_sequence_entry");
  const auto tracks = memory.U32BE(*entry + 4);
  const auto track = memory.U32BE(*entry + 16);
  if (!tracks || *tracks < 2 || !track || !Range(*track, 4))
    return Fail("settings_render_bounds_track");
  const auto records = memory.U16BE(*track);
  if (!records || !*records) return Fail("settings_render_bounds_record_count");
  uint32_t current = *track + 4, previous = current;
  bool selected = false;
  for (uint32_t i = 0; i < *records; ++i) {
    const auto words = memory.U8(current + 1);
    const auto frame = memory.U16BE(current + 2);
    if (!words || !frame || *words < 2 || !Range(current, *words * 4))
      return Fail("settings_render_bounds_record_extent");
    if (!*frame) { selected = true; break; }
    previous = current;
    current += *words * 4;
  }
  if (!selected) return Fail("settings_render_bounds_frame_zero");
  for (const auto record : {previous, current}) {
    const auto words = memory.U8(record + 1);
    const auto kind = memory.U8(record + 4);
    const auto mode = memory.U8(record + 0xD);
    const uint32_t required = type == 1 ? 0x44 : 0x48;
    if (!words || !kind || !mode || *kind != type || *words * 4u < required ||
        !Range(record, required)) return Fail("settings_render_bounds_record_type");
    if (*mode == 2) {
      // Reject overlapping/malformed curve tails before native interpolation.
      if (*words * 4u < required + 36)
        return Fail("settings_render_bounds_interpolation_tail");
      const auto curve = record + *words * 4u - 36;
      for (uint32_t i = 0; i < 9; ++i)
        if (!Float(curve + i * 4)) return Fail("settings_render_bounds_interpolation_value");
    }
  }
  if (type != 4) return true;
  const auto uv_table = memory.U32BE(resource + 0xC);
  const auto uv_index = memory.U16BE(current + 6);
  if (!uv_table || !uv_index || !Range(*uv_table, 8))
    return Fail("settings_render_bounds_uv_table");
  const auto uv_count = memory.U32BE(*uv_table);
  if (!uv_count || !*uv_count || *uv_count > 65535 || *uv_index >= *uv_count ||
      !Range(*uv_table, 8 + *uv_count * 20)) return Fail("settings_render_bounds_uv_count");
  const auto uv = *uv_table + 8 + *uv_index * 20;
  const auto texture_index = memory.U16BE(uv);
  const auto textures = memory.U32BE(auxiliary + 4), texture_count = memory.U32BE(auxiliary + 8);
  if (!Range(auxiliary, 12) || !textures || !texture_count || !texture_index ||
      !*texture_count || *texture_count > 65535 || *texture_index >= *texture_count ||
      !Range(*textures, *texture_count * 20)) return Fail("settings_render_bounds_texture_table");
  const auto descriptor = *textures + *texture_index * 20;
  const auto width = Float(descriptor + 4), height = Float(descriptor + 8);
  const auto top = Float(uv + 4), left = Float(uv + 8);
  const auto bottom = Float(uv + 12), right = Float(uv + 16);
  if (!width || !height || *width <= 0 || *height <= 0 ||
      !left || !top || !right || !bottom || *right <= *left || *bottom <= *top)
    return Fail("settings_render_bounds_texture_dimensions");
  return true;
}

std::optional<int16_t> SignedHalf(uint32_t address) {
  const auto value = memory.U16BE(address);
  return value ? std::optional(std::bit_cast<int16_t>(*value)) : std::nullopt;
}

bool TrackRecords(uint32_t resource, uint32_t sequence, uint32_t track,
                  uint8_t type, std::vector<uint32_t>& records) {
  const auto table = memory.U32BE(resource + 8);
  if (!table || !Range(*table, 8)) return Fail("settings_render_layout_sequence_table");
  const auto count = memory.U32BE(*table);
  if (!count || sequence >= *count || *count > 1024 || !Range(*table, 8 + *count * 4))
    return Fail("settings_render_layout_sequence_count");
  const auto sequence_address = memory.U32BE(*table + 8 + sequence * 4);
  if (!sequence_address || !Range(*sequence_address, 16))
    return Fail("settings_render_layout_sequence");
  const auto tracks = memory.U32BE(*sequence_address + 4);
  if (!tracks || *tracks < 2 || *tracks > 1024 || track >= *tracks - 1 ||
      !Range(*sequence_address, 12 + *tracks * 4)) return Fail("settings_render_layout_tracks");
  const auto first = memory.U32BE(*sequence_address + 16 + track * 4);
  if (!first || !Range(*first, 4)) return Fail("settings_render_layout_track");
  const auto record_count = memory.U16BE(*first);
  if (!record_count || !*record_count) return Fail("settings_render_layout_records");
  uint32_t address = *first + 4;
  uint32_t previous_frame = UINT32_MAX;
  for (uint32_t i = 0; i < *record_count; ++i) {
    const auto words = memory.U8(address + 1), kind = memory.U8(address + 4);
    const auto frame = memory.U16BE(address + 2);
    const uint32_t minimum = type == 3 ? 0x34 : 0x48;
    if (!words || *words * 4u < minimum || !kind || *kind != type || !frame ||
        *frame > previous_frame || !Range(address, *words * 4u))
      return Fail("settings_render_layout_record");
    records.push_back(address);
    previous_frame = *frame;
    address += *words * 4u;
  }
  return true;
}

// 823D3EE8 constructs all four warped corners before applying the record's
// scale, rotation and translation. 823D4700 exposes only the two right corners
// and therefore cannot measure a complete panel or its three component tracks.
bool PanelBounds(uint32_t resource, uint32_t auxiliary, uint32_t sequence, Rect& bounds) {
  const auto table = memory.U32BE(resource + 8);
  const auto entry = table ? memory.U32BE(*table + 8 + sequence * 4) : std::nullopt;
  if (!entry || memory.U32BE(*entry + 4) != 4)
    return Fail("settings_render_panel_track_count");
  bool first_point = true;
  for (uint32_t track = 0; track < 3; ++track) {
    std::vector<uint32_t> records;
    if (!TrackRecords(resource, sequence, track, 4, records)) return false;
    if (records.size() != 1 || memory.U16BE(records[0] + 2) != 0)
      return Fail("settings_render_panel_static_record");
    const auto record = records[0];
    if (memory.U8(record + 0xD) != 0 || memory.U16BE(record + 0x1C) != 0 ||
        memory.U16BE(record + 0x1E) != 0 ||
        memory.U16BE(record + 0x20) != 0)
      return Fail("settings_render_panel_rotation");
    const auto uv_table = memory.U32BE(resource + 0xC);
    const auto uv_index = memory.U16BE(record + 6);
    if (!uv_table || !uv_index || !Range(*uv_table, 8))
      return Fail("settings_render_panel_uv_table");
    const auto uv_count = memory.U32BE(*uv_table);
    if (!uv_count || *uv_count > 65535 || *uv_index >= *uv_count ||
        !Range(*uv_table, 8 + *uv_count * 20)) return Fail("settings_render_panel_uv_count");
    const auto uv = *uv_table + 8 + *uv_index * 20;
    const auto texture = memory.U16BE(uv);
    const auto textures = memory.U32BE(auxiliary + 4), count = memory.U32BE(auxiliary + 8);
    if (!Range(auxiliary, 12) || !textures || !count || !texture || *count > 65535 ||
        *texture >= *count || !Range(*textures, *count * 20))
      return Fail("settings_render_panel_textures");
    const auto width = Float(*textures + *texture * 20 + 4);
    const auto height = Float(*textures + *texture * 20 + 8);
    const auto top = Float(uv + 4), left = Float(uv + 8);
    const auto bottom = Float(uv + 12), right = Float(uv + 16);
    const auto sx = SignedHalf(record + 0x34), sy = SignedHalf(record + 0x36);
    const auto tx = SignedHalf(record + 0x10), ty = SignedHalf(record + 0x12);
    if (!width || !height || *width <= 0 || *height <= 0 || !top || !left || !bottom ||
        !right || *right <= *left || *bottom <= *top || !sx || !sy || *sx <= 0 || *sy <= 0 ||
        !tx || !ty) return Fail("settings_render_panel_dimensions");
    const float w = (*right - *left) * *width, h = (*bottom - *top) * *height;
    for (uint32_t corner = 0; corner < 4; ++corner) {
      const auto x = SignedHalf(record + 0x24 + corner * 4);
      const auto y = SignedHalf(record + 0x26 + corner * 4);
      if (!x || !y) return Fail("settings_render_panel_corner");
      const float px = (*x + (corner < 2 ? w : 0)) * (*sx / 1024.0f) + *tx;
      const float py = (*y + (corner == 1 || corner == 2 ? h : 0)) * (*sy / 1024.0f) + *ty;
      if (!std::isfinite(px) || !std::isfinite(py)) return Fail("settings_render_panel_extent");
      if (first_point) { bounds = {px, py, px, py}; first_point = false; }
      else {
        bounds.left = (std::min)(bounds.left, px); bounds.right = (std::max)(bounds.right, px);
        bounds.top = (std::min)(bounds.top, py); bounds.bottom = (std::max)(bounds.bottom, py);
      }
    }
  }
  return (bounds.width() > 0 && bounds.height() > 0) || Fail("settings_render_panel_empty");
}

struct Pose { float x = 0, y = 0, max_x = 1, max_y = 1; };
bool TrackPose(uint32_t resource, uint32_t sequence, uint32_t track, uint32_t child,
               bool fixed_position, bool unit_scale, Pose& pose) {
  std::vector<uint32_t> records;
  if (!TrackRecords(resource, sequence, track, 3, records)) return false;
  bool first = true;
  for (const auto record : records) {
    const auto x = SignedHalf(record + 0x10), y = SignedHalf(record + 0x12);
    const auto sx = SignedHalf(record + 0x30), sy = SignedHalf(record + 0x32);
    if (!x || !y || !sx || !sy || *sx <= 0 || *sy <= 0 ||
        memory.U16BE(record + 6) != child || memory.U8(record + 0xD) != 0 ||
        memory.U16BE(record + 0x1C) != 0 ||
        memory.U16BE(record + 0x1E) != 0 || memory.U16BE(record + 0x20) != 0 ||
        (unit_scale && (*sx != 1024 || *sy != 1024)) ||
        (!first && fixed_position && (*x != pose.x || *y != pose.y)))
      return Fail("settings_render_layout_parent_pose");
    pose.x = *x; pose.y = *y;
    pose.max_x = (std::max)(pose.max_x, *sx / 1024.0f);
    pose.max_y = (std::max)(pose.max_y, *sy / 1024.0f);
    first = false;
  }
  return true;
}

bool MeasureLayout(PPCContext& ctx, uint8_t* base) {
  constexpr uint32_t matrix_offset = 0x3100, color_offset = 0x3140, bounds_offset = 0x3180;
  const auto manager = memory.U32BE(0x829CDA60);
  if (!manager || !*manager) return Fail("settings_render_context_manager");
  PPCContext call = ctx;
  call.r3.u64 = *manager;
  sub_823A8CE0(call, base);
  const auto backend = memory.U32BE(call.r3.u32 + 4);
  if (!backend || !Range(*backend, 0x130)) return Fail("settings_render_bounds_backend");
  const auto viewport = memory.U32BE(*backend + 0xCC);
  if (!viewport || !Range(*viewport, 0x60)) return Fail("settings_render_bounds_viewport");
  const auto width = memory.U16BE(*viewport + 0x5C), height = memory.U16BE(*viewport + 0x5E);
  if (!width || !height || !*width || !*height) return Fail("settings_render_bounds_dimensions");
  std::array<uint8_t, 64> identity{};
  for (uint32_t i = 0; i < 4; ++i) EncodeWord(identity.data() + i * 20, 0x3F800000);
  std::array<uint8_t, 16> color{};
  for (uint32_t i = 0; i < 4; ++i) EncodeWord(color.data() + i * 4, 0x3F800000);
  if (!Write(state.storage + matrix_offset, identity.data(), identity.size()) ||
      !Write(state.storage + color_offset, color.data(), color.size())) return false;
  std::array<uint8_t, 4> saved_argument{};
  if (!Range(ctx.r1.u32 + 0x54, saved_argument.size()) ||
      !read(ctx.r1.u32 + 0x54, saved_argument.data(), saved_argument.size()))
    return Fail("settings_render_bounds_stack");
  auto measure = [&](uint32_t widget, uint32_t sequence, uint8_t type,
                     float& box_width, float& box_height) {
    const auto resource = memory.U32BE(widget + 0xD0), auxiliary = memory.U32BE(widget + 0xD4);
    if (!resource || !auxiliary || !Range(*resource, 0x18)) return Fail("settings_render_bounds_resource");
    if (!BoundsContract(*resource, *auxiliary, sequence, type)) return false;
    const std::array<uint8_t, 96> zero{};
    if (!Write(state.storage + bounds_offset, zero.data(), zero.size()) || !Word(ctx.r1.u32 + 0x54, 0))
      return false;
    PPCContext bounds = ctx;
    bounds.r3.u64 = *backend;
    bounds.r4.u64 = *resource;
    bounds.r5.u64 = state.storage + bounds_offset;
    bounds.r6.u64 = *auxiliary;
    bounds.r8.u64 = sequence;
    bounds.r9.u64 = state.storage + matrix_offset;
    bounds.r10.u64 = state.storage + color_offset;
    bounds.f1.f64 = 0;
    sub_823C93F0(bounds, base);
    if (!Write(ctx.r1.u32 + 0x54, saved_argument.data(), saved_argument.size())) return false;
    const auto x0 = Float(state.storage + bounds_offset + 8);
    const auto y0 = Float(state.storage + bounds_offset + 0xC);
    const auto x1 = Float(state.storage + bounds_offset + 0x18);
    const auto y1 = Float(state.storage + bounds_offset + 0x1C);
    if (!x0 || !y0 || !x1 || !y1) return Fail("settings_render_bounds_result");
    box_width = std::abs(*x1 - *x0);
    box_height = std::abs(*y1 - *y0);
    if (type == 1)
      return (box_width > 0 && box_height > 0) || Fail("settings_render_label_bounds_empty");
    return box_height > 0 || Fail("settings_render_selection_bounds_empty");
  };
  float label_width = 0, label_height = 0, body_width = 0, body_height = 0;
  if (!measure(state.labels[0], 3, 1, label_width, label_height) ||
      !measure(state.labels[0], 13, 1, body_width, body_height)) return false;
  const auto resource = memory.U32BE(state.labels[0] + 0xD0);
  const auto auxiliary = memory.U32BE(state.labels[0] + 0xD4);
  if (!resource || !auxiliary) return Fail("settings_render_layout_resource");
  Pose root, row, label, zoom;
  if (!TrackPose(*resource, 16, 0, 15, false, true, root) ||
      !TrackPose(*resource, 15, 0, 14, true, true, row) ||
      !TrackPose(*resource, 14, 1, 3, true, true, label) ||
      !TrackPose(*resource, 8, 0, 14, true, false, zoom) || zoom.x != 0 || zoom.y != 0)
    return Fail("settings_render_layout_ancestry");
  state.label_width = label_width;
  state.label_height = label_height;
  state.value_offset = label_width * 1.10f;
  const float panel_width = (state.value_offset + label_width) * 1.10f;
  const float panel_height = label_height * 1.50f;
  float animated_width = 0, animated_height = 0;
  for (uint32_t i = 0; i < 2; ++i) {
    Pose panel;
    if (!TrackPose(*resource, 14, i ? 3 : 4, 10 + i, true, !i, panel) ||
        !PanelBounds(*resource, *auxiliary, 10 + i, state.panel_bounds[i])) return false;
    const auto& bounds = state.panel_bounds[i];
    state.panel_scale_x[i] = panel_width / bounds.width();
    state.panel_scale_y[i] = panel_height / bounds.height();
    state.panel_offset_x[i] = label.x - panel.x + state.value_offset * 0.5f;
    state.panel_offset_y[i] = label.y - panel.y;
    animated_width = (std::max)(animated_width, panel_width * zoom.max_x * panel.max_x);
    animated_height = (std::max)(animated_height, panel_height * zoom.max_y * panel.max_y);
  }
  // Compact list dimensions are presentation choices based on native text.
  // Reserve the full authored zoom/pulse envelope, including transparent ends.
  state.row_step = (std::max)(label_height * 2.30f, animated_height * 1.15f);
  if (state.row_step * (kSlots + 2) > (std::min)(body_height, static_cast<float>(*height)) ||
      animated_width > (std::min)(body_width, static_cast<float>(*width)))
    return Fail("settings_render_layout_does_not_fit");
  return true;
}

using NativeCall = void (*)(PPCContext&, uint8_t*);
void Construct(PPCContext& ctx, uint8_t* base, NativeCall original,
               WidgetKind kind, uint32_t slot, uint32_t table, uint32_t size) {
  const bool managed = state.building && state.model;
  const auto address = ctx.r3.u32;
  if (managed && !state.fault) {
    if (slot >= kSlots || !Range(address, size)) Fail("settings_render_constructor_range");
    else if (kind != WidgetKind::Hidden) {
      const auto which = kind == WidgetKind::Label ? 1u : kind == WidgetKind::Panel ? 2u :
          kind == WidgetKind::Glow ? 3u : 0u;
      if (Template(ctx.r4.u32, which)) {
        ctx.r4.u64 = state.storage + which * kTemplateSize;
        // Resource frame and the five-entry native row map stay in bounds.
        // The selectable's logical identity is restored after construction.
        if (kind == WidgetKind::Selection) ctx.r5.u64 = 0;
      }
    }
  }
  original(ctx, base);
  if (!managed) return;
  if (ctx.r3.u32 != address || memory.U32BE(address) != table) {
    Fail("settings_render_constructor_identity");
    return;
  }
  if (kind == WidgetKind::Selection) {
    if (slot >= kSlots || state.selections[slot]) { Fail("settings_render_selection_duplicate"); return; }
    state.selections[slot] = address;
    if (!Word(address + 0xF4, slot)) return;
  } else if (kind == WidgetKind::Label) {
    if (slot >= kSlots || state.labels[slot]) { Fail("settings_render_label_duplicate"); return; }
    state.labels[slot] = address;
  } else if (kind == WidgetKind::Panel || kind == WidgetKind::Glow) {
    const auto which = kind == WidgetKind::Glow ? 1u : 0u;
    if (slot >= kSlots || state.panels[which][slot]) {
      Fail("settings_render_panel_duplicate"); return;
    }
    state.panels[which][slot] = address;
    if (memory.U16BE(address + 0xD8) != 10 + which ||
        memory.U32BE(address + 0xF4) != slot) { Fail("settings_render_panel_identity"); return; }
  }
  if (!state.widgets.emplace(address, Widget{kind, slot, table}).second)
    Fail("settings_render_widget_duplicate");
}

}  // namespace

void SetNativeSettingsRenderAccess(ui::ReadBytes reader,
    std::function<bool(uint32_t, const void*, size_t)> writer) {
  read = std::move(reader);
  write = std::move(writer);
  memory = ui::GuestUiMemory(read);
}

bool NativeSettingsRenderBegin(uint32_t model, PPCContext& ctx, uint8_t* base) {
  auto reject = [](std::string_view reason) { QueueGameDiagnostic(reason); return false; };
  if (state.model) return reject("settings_render_already_owned");
  if (!read || !write) return reject("settings_render_access_missing");
  if (model < 0x20 || memory.U32BE(model - 0x20) != 0x82019C40)
    return reject("settings_render_task_identity");
  if (!Range(model, 0x30)) return reject("settings_render_model_range");
  // 82334FC0 stores the numeric context value 2 at model+0. The builder
  // obtains the shared title/hint owner independently through this global.
  const auto common = memory.U32BE(0x829CBC68);
  if (!common || !*common) return reject("settings_render_common_anchor");
  if (memory.U32BE(*common) != 0x82003EAC) return reject("settings_render_common_identity");
  if (!Range(*common, 0xE0)) return reject("settings_render_common_range");
  PPCContext allocation = ctx;
  allocation.r3.u64 = kStorageSize;
  allocation.r4.u64 = 1;
  sub_8212E940(allocation, base);
  const auto storage = allocation.r3.u32;
  if (!storage) return reject("settings_render_allocation_failed");
  if (storage % 16 || !Range(storage, kStorageSize)) {
    allocation.r3.u64 = storage;
    sub_8212EAD0(allocation, base);
    return reject("settings_render_allocation_range");
  }
  state = {};
  state.model = model;
  state.common = *common;
  state.storage = storage;
  state.building = true;
  const std::array<uint8_t, kStorageSize> zero{};
  if (Write(storage, zero.data(), zero.size())) return true;
  NativeSettingsRenderEnd(model, ctx, base);
  return false;
}

bool NativeSettingsRenderBuilt(uint32_t model, PPCContext& ctx, uint8_t* base) {
  if (model != state.model || !state.building) return Fail("settings_render_build_owner");
  state.building = false;
  if (state.fault) return false;
  if (!std::all_of(state.templates.begin(), state.templates.end(), [](bool value) { return value; }) ||
      state.values != 7 ||
      state.pairs != 10 || state.actions != 2 || state.widgets.size() != 29)
    return Fail("settings_render_build_counts");
  for (uint32_t slot = 0; slot < kSlots; ++slot)
    if (!state.labels[slot] || !state.selections[slot] ||
        !state.panels[0][slot] || !state.panels[1][slot]) return Fail("settings_render_build_slot");
  for (const auto& panels : state.panels)
    for (const auto panel : panels)
      if (memory.U32BE(panel + 0xD0) != memory.U32BE(state.labels[0] + 0xD0) ||
          memory.U32BE(panel + 0xD4) != memory.U32BE(state.labels[0] + 0xD4))
        return Fail("settings_render_panel_resource_owner");
  if (!MeasureLayout(ctx, base)) return false;
  state.built = true;
  return true;
}

bool NativeSettingsRenderPublish(const View& view) {
  if (!state.model || state.fault) return false;
  if ((view.rows.empty() && view.position) || (!view.rows.empty() &&
      (!view.position || view.position > view.rows.size()))) return Fail("settings_render_view_position");
  size_t first = state.first;
  if (view.page != state.view.page || view.confirming != state.view.confirming) first = 0;
  if (!view.rows.empty()) {
    const auto selected = view.position - 1;
    if (selected < first) first = selected;
    if (selected >= first + kSlots) first = selected - kSlots + 1;
    first = (std::min)(first, view.rows.size() > kSlots ? view.rows.size() - kSlots : 0);
  } else first = 0;
  std::array<std::string_view, 15> strings{};
  uint32_t visible = static_cast<uint32_t>((std::min)(size_t{kSlots}, view.rows.size() - first));
  uint32_t selected = view.rows.empty() ? 0 : static_cast<uint32_t>(view.position - 1 - first);
  if (view.confirming) {
    strings[0] = view.confirmation;
    strings[2] = view.yes;
    strings[4] = view.no;
    visible = 3;
    selected = view.confirm_yes ? 1 : 2;
  } else {
    for (uint32_t slot = 0; slot < visible; ++slot) {
      strings[slot * 2] = view.rows[first + slot].label;
      strings[slot * 2 + 1] = view.rows[first + slot].value;
    }
    if (view.position) strings[kDescriptionText] = view.rows[view.position - 1].description;
  }
  strings[kTitleText] = view.title;
  strings[kHintsText] = view.hints;
  strings[kSectionText] = view.section;
  std::string footer(strings[kDescriptionText]);
  if (!view.notice.empty()) {
    if (!footer.empty()) footer += "\n";
    footer += view.notice;
  }
  strings[kDescriptionText] = footer;
  std::array<std::array<uint8_t, kTextSize>, 15> encoded{};
  for (size_t i = 0; i < strings.size(); ++i)
    if (!EncodeText(strings[i], encoded[i])) return Fail("settings_render_caption_encoding");
  View copy = view;
  for (uint32_t i = 0; i < encoded.size(); ++i)
    if (!Write(TextAddress(i), encoded[i].data(), encoded[i].size())) return false;
  state.view = std::move(copy);
  state.first = first;
  state.visible = visible;
  state.selected = selected;
  return true;
}

uint32_t NativeSettingsRenderSelection() { return state.selected; }
bool NativeSettingsRenderHealthy() { return state.model && !state.fault; }
void NativeSettingsRenderSuspend(bool suspended) { state.suspended = suspended; }

void NativeSettingsRenderEnd(uint32_t model, PPCContext& ctx, uint8_t* base) {
  if (!state.model || model != state.model) return;
  const auto storage = state.storage;
  state = {};
  text_scope = {};
  if (storage) {
    PPCContext release = ctx;
    release.r3.u64 = storage;
    sub_8212EAD0(release, base);
  }
}

void NativeSettingsRenderProvider(PPCContext& ctx, uint32_t bank, uint32_t id, uint64_t) {
  if (!state.model || text_scope.kind == TextKind::None || bank) return;
  uint32_t text = kEmptyText;
  if (text_scope.kind == TextKind::Label) {
    if (text_scope.slot >= kSlots || id != kLabelIds[text_scope.slot]) {
      Fail("settings_render_label_provider");
      return;
    }
    if (Present(text_scope.slot)) text = text_scope.slot * 2;
  } else if (text_scope.kind == TextKind::Title) {
    if (id != 0x612) return;
    if (state.view.open && !state.suspended && !state.fault) text = kTitleText;
  } else {
    if (id != 0xA21 && id != 0xA1E) return;
    if (state.view.open && !state.suspended && !state.fault) text = kHintsText;
  }
  ctx.r3.u64 = TextAddress(text);
}

bool NativeSettingsRenderHints(PPCContext& ctx, uint8_t* base) {
  if (!state.model || state.suspended ||
      Record(ctx.r3.u32, ctx.r4.u32) != state.common + 0x94) return false;
  const auto id = memory.U32BE(state.common + 0xC4);
  if (id != 0xA21 && id != 0xA1E) return false;
  ScopedText scope(TextKind::Hints);
  __imp__sub_821EA650(ctx, base);
  return true;
}

bool NativeSettingsRenderText(PPCContext& ctx, uint8_t* base) {
  if (!state.model || text_scope.kind == TextKind::None) return false;
  if (state.fault || state.suspended || !state.view.open) return true;
  if (text_scope.kind == TextKind::Hints) {
    ScopedFontFit fit;
    __imp__sub_823A9528(ctx, base);
    return true;
  }
  if (text_scope.kind != TextKind::Label) return false;
  const auto slot = text_scope.slot;
  if (!Present(slot)) return true;
  const auto initial = ctx;
  struct RestoreBox {
    TextBox previous = text_box;
    ~RestoreBox() { text_box = previous; }
  } restore_box;
  if (state.view.confirming) text_box = slot == 0 ? TextBox::Wide : TextBox::Button;
  else text_box = state.view.rows[state.first + slot].value.empty() ? TextBox::Button : TextBox::Row;
  ctx.r4.u64 = TextAddress(slot * 2);
  {
    ScopedFontFit fit;
    __imp__sub_823A9528(ctx, base);
  }
  auto draw = [&](uint32_t text, float x, float y, TextBox box, bool parent_pose = false) {
    std::array<uint8_t, 64> matrix{};
    // The label's +30 matrix contains its parent opening/closing pose. +70,
    // copied to the normal text call, also includes the row's focus zoom.
    const auto source = parent_pose ? state.labels[0] + 0x30 : initial.r9.u32;
    if (!Range(source, matrix.size()) || !read(source, matrix.data(), matrix.size()) ||
        !Write(state.storage + kMatrix, matrix.data(), matrix.size()))
      return Fail("settings_render_text_matrix");
    if (!AddFloat(state.storage + kMatrix + 0x30, x) ||
        !AddFloat(state.storage + kMatrix + 0x34, y)) return false;
    PPCContext value = initial;
    value.r4.u64 = TextAddress(text);
    value.r9.u64 = state.storage + kMatrix;
    text_box = box;
    ScopedFontFit fit;
    __imp__sub_823A9528(value, base);
    return true;
  };
  // Additional text draws reuse the native font/style without advancing the
  // owning label's animation callback a second time.
  if (!state.view.confirming && !state.view.rows[state.first + slot].value.empty() &&
      !draw(slot * 2 + 1, state.value_offset, 0, TextBox::Row)) return true;
  if (slot == 0) {
    if (!state.view.section.empty() &&
        !draw(kSectionText, 0, -state.row_step, TextBox::Wide, true)) return true;
    if (!draw(kDescriptionText, 0, state.row_step * state.visible, TextBox::Wide, true)) return true;
  }
  return true;
}

}  // namespace brawlers::settings

extern "C" void sub_82336850(PPCContext& ctx, uint8_t* base) {
  using namespace brawlers::settings;
  Construct(ctx, base, __imp__sub_82336850, WidgetKind::Selection, ctx.r5.u8, 0x82019DA0, 0x120);
}
extern "C" void sub_82336B00(PPCContext& ctx, uint8_t* base) {
  using namespace brawlers::settings;
  Construct(ctx, base, __imp__sub_82336B00, WidgetKind::Label, ctx.r5.u8, 0x82019DB4, 0x110);
}
extern "C" void sub_82337128(PPCContext& ctx, uint8_t* base) {
  using namespace brawlers::settings;
  if (state.building) ++state.values;
  Construct(ctx, base, __imp__sub_82337128, WidgetKind::Hidden, ctx.r6.u8, 0x82019DD0, 0x120);
}
extern "C" void sub_823375D8(PPCContext& ctx, uint8_t* base) {
  using namespace brawlers::settings;
  if (state.building) ++state.values;
  Construct(ctx, base, __imp__sub_823375D8, WidgetKind::Hidden, ctx.r5.u8, 0x82019DE4, 0x120);
}
extern "C" void sub_82336E60(PPCContext& ctx, uint8_t* base) {
  using namespace brawlers::settings;
  if (state.building) ++state.pairs;
  const auto sequence = memory.U32BE(ctx.r4.u32 + 0x1C);
  const auto kind = sequence == 10 ? WidgetKind::Panel : sequence == 11 ? WidgetKind::Glow : WidgetKind::Hidden;
  if (state.building && kind == WidgetKind::Hidden) Fail("settings_render_panel_sequence");
  Construct(ctx, base, __imp__sub_82336E60, kind, ctx.r5.u8, 0x82019DBC, 0x130);
}
extern "C" void sub_821CDAC0(PPCContext& ctx, uint8_t* base) {
  using namespace brawlers::settings;
  const bool managed = state.building;
  const auto address = ctx.r3.u32;
  __imp__sub_821CDAC0(ctx, base);
  if (managed) {
    ++state.actions;
    if (ctx.r3.u32 != address || !Range(address, 0x130)) {
      Fail("settings_render_action_constructor");
      return;
    }
    // The builder installs the Options-specific table immediately after this
    // constructor; validate that table at render time, after the store.
    state.widgets.emplace(address, Widget{WidgetKind::Hidden, 0, 0x82019DF8});
  }
}
extern "C" void sub_82336C28(PPCContext& ctx, uint8_t* base) {
  using namespace brawlers::settings;
  const auto found = state.widgets.find(ctx.r3.u32);
  if (found == state.widgets.end() || found->second.kind != WidgetKind::Label) {
    __imp__sub_82336C28(ctx, base);
    return;
  }
  if (!Identity(found->first, found->second)) {
    Fail("settings_render_label_identity");
    __imp__sub_82336C28(ctx, base);
    return;
  }
  ScopedText scope(TextKind::Label, found->second.slot);
  __imp__sub_82336C28(ctx, base);
}
extern "C" void sub_821E9908(PPCContext& ctx, uint8_t* base) {
  using namespace brawlers::settings;
  if (!state.model || state.suspended || memory.U32BE(state.common + 0x58) != 9 ||
      memory.U32BE(ctx.r3.u32) != 0x82003EBC ||
      Record(ctx.r3.u32, ctx.r4.u32) != state.common + 0x4C) {
    __imp__sub_821E9908(ctx, base);
    return;
  }
  ScopedText scope(TextKind::Title);
  __imp__sub_821E9908(ctx, base);
}
extern "C" void sub_823C64A0(PPCContext& ctx, uint8_t* base) {
  using namespace brawlers::settings;
  const auto found = state.widgets.find(ctx.r3.u32);
  if (found == state.widgets.end()) { __imp__sub_823C64A0(ctx, base); return; }
  const auto widget = found->first;
  const auto item = found->second;
  if (!Identity(widget, item)) {
    Fail("settings_render_widget_identity");
    __imp__sub_823C64A0(ctx, base);
    return;
  }
  const auto visible = memory.U8(widget + 0xDA);
  if (!visible) { Fail("settings_render_visibility_field"); __imp__sub_823C64A0(ctx, base); return; }
  const bool hide = item.kind == WidgetKind::Hidden || item.kind == WidgetKind::Selection ||
      !Present(item.slot) || (state.view.confirming && item.slot == 0 && item.kind != WidgetKind::Label);
  if (hide) { const uint8_t zero = 0; Write(widget + 0xDA, &zero, 1); }
  __imp__sub_823C64A0(ctx, base);
  if (hide) Write(widget + 0xDA, &*visible, 1);
}
extern "C" void sub_823C93F0(PPCContext& ctx, uint8_t* base) {
  using namespace brawlers::settings;
  const auto output = ctx.r5.u32;
  const bool owned_box = text_scope.kind == TextKind::Label &&
      (text_box == TextBox::Button || text_box == TextBox::Wide) && ctx.r8.u32 == 3 && !state.fault;
  __imp__sub_823C93F0(ctx, base);
  if (!owned_box) return;
  // Native alignment mode one centers a caption in its measured box. Buttons
  // occupy both columns; value rows retain their two native-width boxes.
  // Preserve focus zoom around the full button center, whose column offset is
  // a world-space translation like the panel's offset.
  const auto x0 = Float(output + 8), y0 = Float(output + 0xC);
  const auto x1 = Float(output + 0x18), y1 = Float(output + 0x1C);
  if (!x0 || !y0 || !x1 || !y1 || state.label_width <= 0 || *x0 == *x1) {
    Fail("settings_render_text_bounds"); return;
  }
  const float width = std::abs(*x0 - *x1) * (state.value_offset + state.label_width) / state.label_width;
  const float center = (*x0 + *x1 + state.value_offset) * 0.5f;
  const float half = std::copysign(width * 0.5f, *x0 - *x1);
  if (!std::isfinite(width) || !std::isfinite(center)) { Fail("settings_render_text_bounds"); return; }
  if (!Word(output + 8, std::bit_cast<uint32_t>(center + half))) return;
  Word(output + 0x18, std::bit_cast<uint32_t>(center - half));
}
extern "C" void sub_82396140(PPCContext& ctx, uint8_t* base) {
  using namespace brawlers::settings;
  // 823A9528 measures through 823966A0 (r9=0) before drawing (r9=1).
  // Its vertical alignment consumes that measured height. One operation must
  // use the same fit during both passes, including the line advance in f4.
  const bool hints = text_scope.kind == TextKind::Hints;
  const bool row = text_scope.kind == TextKind::Label && text_box != TextBox::Native;
  if ((!row && !hints) || !font_fit.active || state.fault) {
    __imp__sub_82396140(ctx, base);
    return;
  }
  const auto manager = ctx.r3.u32;
  if (memory.U32BE(0x829CD808) != manager || !Range(manager, 0x318)) {
    Fail("settings_render_font_manager");
    return;
  }
  const auto scale_x = Float(manager + 0x28), scale_y = Float(manager + 0x2C);
  const auto first_line = memory.U32BE(manager + 0x30C), line_limit = memory.U32BE(manager + 0x310);
  const auto glyphs = memory.U32BE(manager + 0xC);
  const auto style = memory.U8(manager + 0x314);
  if (!scale_x || !scale_y || !first_line || !line_limit ||
      *scale_x <= 0 || *scale_y <= 0 || !glyphs || !style ||
      !Range(*glyphs + static_cast<uint32_t>(*style) * 0x58, 0x58)) {
    Fail("settings_render_font_style");
    return;
  }
  uint32_t lines = 1;
  bool terminated = false, has_text = false;
  for (uint32_t i = 0; i < kTextSize / 2; ++i) {
    const auto unit = memory.U16BE(ctx.r4.u32 + i * 2);
    if (!unit) { Fail("settings_render_font_text"); return; }
    if (!*unit) { terminated = true; break; }
    has_text = true;
    if (*unit == 10) ++lines;
  }
  if (!terminated || !std::isfinite(ctx.f3.f64) || ctx.f3.f64 <= 0 ||
      !std::isfinite(ctx.f4.f64) || ctx.f4.f64 <= 0) {
    Fail("settings_render_font_limits");
    return;
  }
  // The native empty-input return precedes its metric reset. Publish an empty
  // layout explicitly so 823966A0 cannot reuse the preceding caption's height.
  if (!has_text) {
    if (Word(manager + 0x304, 0) && Word(manager + 0x308, 0)) ctx.r3.u64 = 0;
    return;
  }
  struct RestoreFont {
    uint32_t manager, x, y, first, limit;
    ~RestoreFont() {
      Word(manager + 0x28, x);
      Word(manager + 0x2C, y);
      Word(manager + 0x30C, first);
      Word(manager + 0x310, limit);
    }
  } restore{manager, std::bit_cast<uint32_t>(*scale_x), std::bit_cast<uint32_t>(*scale_y),
            *first_line, *line_limit};
  if (!Word(manager + 0x30C, 0) || !Word(manager + 0x310, 0)) return;
  if (!font_fit.factor) {
    if (ctx.r9.u8 != 0) { Fail("settings_render_font_pass_order"); return; }
    double fit = 1;
    const auto available_height = hints ? ctx.f4.f64 : state.row_step * 0.85;
    if (row && text_box == TextBox::Wide) {
      // Let the native word wrapper use the full width before reducing size.
      // Its +304 result includes the native glyph height and line advances.
      PPCContext measure = ctx;
      __imp__sub_82396140(measure, base);
      const auto height = Float(manager + 0x304);
      if (!height || *height < 0) { Fail("settings_render_font_measure"); return; }
      if (*height > available_height) fit = available_height / *height;
    } else {
      PPCContext measure = ctx;
      measure.r5.u64 = state.storage + 0x3200;
      measure.r6.u64 = state.storage + 0x3204;
      sub_823958B8(measure, base);
      const auto width = Float(state.storage + 0x3200), height = Float(state.storage + 0x3204);
      if (!width || !height || *width < 0 || *height < 0) { Fail("settings_render_font_measure"); return; }
      const auto requested_height = (std::max)(static_cast<double>(*height), ctx.f4.f64 * lines);
      if (*width > 0) fit = (std::min)(fit, ctx.f3.f64 * 0.98 / *width);
      if (requested_height > 0) fit = (std::min)(fit, available_height / requested_height);
    }
    if (!std::isfinite(fit) || fit <= 0) { Fail("settings_render_font_fit"); return; }
    font_fit.factor = fit;
  }
  const double fit = *font_fit.factor;
  const auto fitted_x = static_cast<float>(*scale_x * fit);
  const auto fitted_y = static_cast<float>(*scale_y * fit);
  if (!Word(manager + 0x28, std::bit_cast<uint32_t>(fitted_x)) ||
      !Word(manager + 0x2C, std::bit_cast<uint32_t>(fitted_y))) return;
  ctx.f4.f64 *= fit;
  __imp__sub_82396140(ctx, base);
}
void NativeSettingsRowTransform(PPCRegister& owner) {
  using namespace brawlers::settings;
  const auto found = state.widgets.find(owner.u32);
  if (!state.built || found == state.widgets.end() || found->second.kind == WidgetKind::Hidden ||
      found->second.kind == WidgetKind::Selection || state.fault) return;
  if (!Identity(found->first, found->second)) { Fail("settings_render_transform_identity"); return; }
  // A repeated first-row template supplies identical native animation motion.
  // The final matrix is rebuilt each draw, so this offset cannot accumulate.
  const auto widget = found->first;
  const auto kind = found->second.kind;
  if (kind == WidgetKind::Panel || kind == WidgetKind::Glow) {
    const auto which = kind == WidgetKind::Glow ? 1u : 0u;
    const auto& bounds = state.panel_bounds[which];
    const float center_x = (bounds.left + bounds.right) * 0.5f;
    const float center_y = (bounds.top + bounds.bottom) * 0.5f;
    // Scale the native basis, preserving its focus/pulse and parent motion.
    // Center the full native panel around the new label/value area.
    std::array<float, 8> basis{};
    for (uint32_t i = 0; i < basis.size(); ++i) {
      const auto value = Float(widget + 0x70 + i * 4);
      if (!value) { Fail("settings_render_panel_matrix"); return; }
      basis[i] = *value * (i < 4 ? state.panel_scale_x[which] : state.panel_scale_y[which]);
      if (!std::isfinite(basis[i])) { Fail("settings_render_panel_matrix_extent"); return; }
    }
    for (uint32_t i = 0; i < basis.size(); ++i)
      if (!Word(widget + 0x70 + i * 4, std::bit_cast<uint32_t>(basis[i]))) return;
    if (!AddFloat(widget + 0xA0, state.panel_offset_x[which] - center_x * basis[0] - center_y * basis[4]) ||
        !AddFloat(widget + 0xA4, state.panel_offset_y[which] - center_x * basis[1] - center_y * basis[5]))
      return;
  }
  AddFloat(widget + 0xA4, state.row_step * static_cast<float>(found->second.slot));
}
