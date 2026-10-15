// Behavioral integration for the verified Options task, separate from speech
// observations. Native ownership and consumer evidence: docs/settings.md.
#include "native_settings.h"

#include "native_game_ui.h"
#include "native_localization.h"
#include "native_settings_render.h"
#include "native_settings_save.h"
#include "settings_catalog.h"
#include "settings_events.h"
#include "../runtime/settings_runtime.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <memory>
#include <map>
#include <set>

#include <rex/ppc/context.h>
#include <rex/system/xmemory.h>

#define ORIGINAL(address) extern "C" void __imp__sub_##address(PPCContext&, uint8_t*)
ORIGINAL(82335320);
ORIGINAL(823351E8);
ORIGINAL(82333E38);
ORIGINAL(82333CD0);
ORIGINAL(82334E50);
ORIGINAL(82334888);
ORIGINAL(823341A0);
ORIGINAL(821D7628);
ORIGINAL(821D7670);
ORIGINAL(821C0880);
ORIGINAL(821C1DA8);
ORIGINAL(822E5D38);
ORIGINAL(822E6288);
ORIGINAL(822E6560);
ORIGINAL(821BC008);
ORIGINAL(821CBF18);
ORIGINAL(823ADFA8);
ORIGINAL(821EB330);
ORIGINAL(82212018);
#undef ORIGINAL
extern "C" void sub_821DCD78(PPCContext&, uint8_t*);

namespace {
using namespace brawlers::settings;
using brawlers::ui::GuestUiMemory;
rex::memory::Memory* guest_memory = nullptr;
HostSettingsRuntime* host = nullptr;
GuestUiMemory memory({});
Menu menu;
Events events;
uint64_t next_identity = 0;
std::set<std::string> reported;
std::set<std::string> pending_notices;
thread_local bool skip_final_save = false;
thread_local uint32_t updating_task = 0;
std::map<uint32_t, uint32_t> skipped_final_saves;

struct SaveLifetime { uint32_t common = 0; bool detached = false; };
struct RetainedSave {
  uint32_t storage;
  std::shared_ptr<SaveLifetime> lifetime;
  std::unique_ptr<NativeSettingsSave> saves;
};
std::vector<RetainedSave> retained_saves;

struct Session {
  uint32_t task = 0, model = 0, primary = 0, secondary = 0, selector = 0;
  uint32_t common = 0, locale = 0, storage = 0;
  uint64_t identity = 0;
  Catalog catalog;
  std::unique_ptr<NativeSettingsSave> saves;
  std::shared_ptr<SaveLifetime> save_lifetime;
  bool opened = false, closing = false, failed = false, suspended = false;
  bool quarantine = false;
  bool save_fault_reported = false;
  uint64_t catalog_revision = 0;
  std::optional<uint64_t> host_command;
  std::string notice;
};
std::unique_ptr<Session> session;
// Original calls use the current guest stack, never a host-thread PPC context.
thread_local PPCContext* call_context = nullptr;
thread_local uint8_t* call_base = nullptr;

class GuestCallScope {
 public:
  GuestCallScope(PPCContext& ctx, uint8_t* base)
      : previous_(call_context), previous_base_(call_base) {
    call_context = &ctx; call_base = base;
  }
  ~GuestCallScope() { call_context = previous_; call_base = previous_base_; }
 private:
  PPCContext* previous_;
  uint8_t* previous_base_;
};

bool Guard(bool condition, const char* reason) {
  if (!condition && reported.emplace(reason).second) QueueGameDiagnostic(reason);
  return condition;
}

bool Read(uint32_t address, void* target, size_t size) {
  return guest_memory && guest_memory->TryReadVirtual(address, target, size);
}

// Writes target only task-owned fields whose native consumers also write them,
// or this integration's dedicated guest allocations. Readability alone does not
// establish ownership; callers validate each owner before reaching this seam.
bool Write(uint32_t address, const void* source, size_t size) {
  if (!size || size > 65536) return false;
  std::vector<uint8_t> previous(size);
  if (!Read(address, previous.data(), size)) return false;
  std::memcpy(guest_memory->TranslateVirtual<uint8_t*>(address), source, size);
  return true;
}

bool Byte(uint32_t address, uint8_t value) { return Write(address, &value, 1); }

bool Object(uint32_t address, uint32_t table, const char* reason) {
  return Guard(address && address < UINT32_MAX - 0x100 && memory.U32BE(address) == table, reason);
}

bool OwnsModel(const Session& s) {
  if (!Object(s.task, 0x82019C40, "settings_task_owner") || s.model != s.task + 0x20)
    return false;
  return Guard(memory.U32BE(s.model + 0x14) == s.primary, "settings_primary_owner") &&
      Guard(memory.U32BE(s.model + 0x18) == s.secondary, "settings_secondary_owner") &&
      Object(s.primary, 0x82019D1C, "settings_primary_record") &&
      Object(s.secondary, 0x82019D1C, "settings_secondary_record") &&
      Guard(memory.U32BE(s.model + 0xC) == s.selector, "settings_selector_owner") &&
      Object(s.selector, 0x8200FC30, "settings_selector_table") &&
      Guard(memory.U32BE(s.primary + 0xC) == s.selector, "settings_record_selector") &&
      Guard(memory.U32BE(s.primary + 0x14) == s.model + 0x1C, "settings_record_player") &&
      Guard(memory.U32BE(s.primary + 0x18) == s.model + 0x24, "settings_record_values") &&
      Guard(memory.U32BE(s.primary + 0x1C) == s.task + 0x54, "settings_record_preview") &&
      Guard(memory.U32BE(s.secondary + 0x18) == s.model + 0x24, "settings_secondary_values");
}

bool NativeContext() {
  if (!session || !OwnsModel(*session) || !call_context || !call_base) return false;
  const auto phase = memory.U32BE(session->task + 0x14);
  return Guard(phase && *phase == 2, "settings_save_task_phase") &&
      Guard(memory.U32BE(0x829CBC68) == session->common, "settings_common_owner") &&
      Object(session->common, 0x82003EAC, "settings_common_table");
}

bool SaveContext(const SaveLifetime& lifetime) {
  if (!lifetime.detached) return NativeContext();
  // The owned adapter has no task pointer. Native retries may outlive an
  // unexpectedly removed Options task, but still require the original common
  // UI and controller system. The adapter verifies its service/image/job chain.
  const auto input = memory.U32BE(0x829CDA70);
  return call_context && call_base &&
      Guard(memory.U32BE(0x829CBC68) == lifetime.common, "settings_retained_common_owner") &&
      Object(lifetime.common, 0x82003EAC, "settings_retained_common_table") &&
      Guard(input.has_value(), "settings_retained_input_anchor") &&
      Object(*input, 0x82035B48, "settings_retained_input_table");
}

using GuestFunction = void (*)(PPCContext&, uint8_t*);
uint32_t Call(GuestFunction function, uint32_t r3 = 0, uint32_t r4 = 0, uint32_t r5 = 0) {
  auto ctx = *call_context;
  ctx.r3.u64 = r3; ctx.r4.u64 = r4; ctx.r5.u64 = r5;
  function(ctx, call_base);
  return ctx.r3.u32;
}

bool SaveCallOriginal(SaveCall kind, uint32_t r3, uint32_t r4, uint32_t r5, uint32_t& result) {
  GuestFunction function = nullptr;
  switch (kind) {
    case SaveCall::Request: function = __imp__sub_821C0880; break;
    case SaveCall::Pump: function = __imp__sub_821C1DA8; break;
    case SaveCall::WriteRam: function = __imp__sub_822E5D38; break;
    case SaveCall::DisableContext: function = __imp__sub_821BC008; break;
  }
  if (!function || !call_context) return false;
  result = Call(function, r3, r4, r5);
  return true;
}

bool ReadableRange(uint32_t address, uint32_t size, const char* reason) {
  // A validation budget, not an inferred native array length.
  if (!Guard(address && size && size <= 65536 && address <= UINT32_MAX - size, reason)) return false;
  std::array<uint8_t, 256> bytes{};
  for (uint32_t offset = 0; offset < size; offset += bytes.size())
    if (!Guard(Read(address + offset, bytes.data(), std::min<size_t>(bytes.size(), size - offset)), reason))
      return false;
  return true;
}

bool FiniteFloat(uint32_t address, const char* reason) {
  const auto bits = memory.U32BE(address);
  return Guard(bits && std::isfinite(std::bit_cast<float>(*bits)), reason);
}

bool AudioContext(std::optional<uint32_t> sound = std::nullopt) {
  const auto audio = memory.U32BE(0x829CDCE8);
  if (!Guard(audio.has_value(), "settings_audio_anchor") ||
      !Object(*audio, 0x82036260, "settings_audio_manager") ||
      !ReadableRange(*audio, 0x2B4, "settings_audio_manager_fields")) return false;
  const auto child = memory.U32BE(*audio + 0x40);
  const auto pool = memory.U32BE(*audio + 0x3C);
  const auto metadata = memory.U32BE(*audio + 0x2AC), count = memory.U32BE(*audio + 0x2B0);
  if (!Guard(child && pool && metadata && count, "settings_audio_child_fields") ||
      !Object(*child, 0x82036390, "settings_audio_child_table") ||
      !Guard(memory.U32BE(*child + 4) == *audio + 0x58, "settings_audio_child_engine") ||
      !Guard(memory.U32BE(*child + 8) == *pool, "settings_audio_child_pool") ||
      !Guard(memory.U32BE(*child + 0xC) == *audio + 0x29C, "settings_audio_child_groups") ||
      !Guard(memory.U32BE(*child + 0x10) == *metadata, "settings_audio_child_metadata") ||
      !Guard(memory.U32BE(*child + 0x14) == *count, "settings_audio_child_count") ||
      !ReadableRange(*pool, 0x28, "settings_audio_pool_fields")) return false;
  const auto capacity = memory.U32BE(*pool + 4), begin = memory.U32BE(*pool + 0x14);
  const auto end = memory.U32BE(*pool + 0x18), cursor = memory.U32BE(*pool + 0x24);
  if (!Guard(capacity && *capacity && begin && end && cursor && *end >= *begin &&
      (*end - *begin) % 4 == 0 && (*end - *begin) / 4 >= *capacity - 1 &&
      *cursor <= (*end - *begin) / 4, "settings_audio_pool_layout")) return false;
  if (*end != *begin && !ReadableRange(*begin, *end - *begin, "settings_audio_pool_entries")) return false;
  if (!FiniteFloat(*audio + 0x4C, "settings_audio_sfx_gain") ||
      !FiniteFloat(*audio + 0x50, "settings_audio_voice_gain")) return false;
  if (sound && *sound < *count) {
    const uint64_t address = uint64_t(*metadata) + uint64_t(*sound) * 12;
    if (!Guard(*metadata && address <= UINT32_MAX - 12, "settings_audio_sound_metadata_address")) return false;
    const auto entry = static_cast<uint32_t>(address);
    if (!ReadableRange(entry, 12, "settings_audio_sound_metadata") ||
        !FiniteFloat(entry, "settings_audio_sound_gain") ||
        !FiniteFloat(entry + 4, "settings_audio_sound_pitch")) return false;
    const auto groups = memory.U32BE(*audio + 0x29C), group = memory.U32BE(entry + 8);
    if (!Guard(groups && group && *groups && *groups <= UINT32_MAX - 4096,
               "settings_audio_sound_group_owner") ||
        !ReadableRange(*groups + (*group & 0xFF) * 16, 16, "settings_audio_sound_group")) return false;
  }
  return true;
}

void Sound(uint32_t sound) {
  if (AudioContext(sound)) Call(sub_821DCD78, sound, 0);
}

void StopVoice() {
  if (!session || !AudioContext() || !memory.U32BE(session->secondary + 0x20).value_or(0)) return;
  auto ctx = *call_context;
  ctx.r3.u64 = session->secondary + 0x20;
  ctx.f1.f64 = 0.0;
  __imp__sub_821EB330(ctx, call_base);
}

void PreviewVoice() {
  if (!session || !AudioContext()) return;
  const auto seed = Call(__imp__sub_821CBF18);
  if (!Object(seed, 0x820F19E0, "settings_voice_random_owner")) return;
  const auto index = Call(__imp__sub_823ADFA8, seed, 0, 0x10);
  if (!Guard(index <= 0x10, "settings_voice_preview_index")) return;
  const auto sound = memory.U32BE(0x82019CD8 + index * 4);
  if (!Guard(sound && *sound >= 0x51D && *sound <= 0x52D, "settings_voice_preview_id")) return;
  if (!AudioContext(*sound)) return;
  StopVoice();
  Call(__imp__sub_82212018, *sound, session->secondary + 0x20);
}

View CurrentView() {
  auto view = menu.Read();
  if (session) view.notice = session->notice;
  return view;
}

bool SynchronizeSelection(uint32_t slot) {
  if (!session || !call_context || !call_base || !OwnsModel(*session)) return false;
  // 8226FC08 owns the list at selector+40; 821DED48 binds it at +28 and
  // initially disables selection. 821D7628 updates its index, but the visual
  // query 8226FB80 also requires the list's +18 enabled byte. Native input
  // polling normally enables it through 821DEF3C; managed input replaces that
  // polling, so restore only this presentation state through its native setter.
  const auto child = memory.U32BE(session->selector + 0x28);
  if (!Guard(child && *child == session->selector + 0x40, "settings_selector_list_owner") ||
      !Object(*child, 0x8200FC10, "settings_selector_list_table") ||
      !ReadableRange(*child, 0x1C, "settings_selector_list_range")) return false;
  const auto count = memory.U32BE(*child + 0x14), index = memory.U32BE(*child + 0x10);
  const auto enabled = memory.U8(*child + 0x18);
  if (!Guard(count && *count == 7 && index && *index < *count && slot < 5,
             "settings_selector_list_index") ||
      !Guard(enabled && *enabled <= 1, "settings_selector_list_enabled")) return false;
  Call(__imp__sub_821D7628, session->selector, slot);
  if (!Guard(memory.U32BE(*child + 0x10) == slot, "settings_selector_index_result")) return false;
  if (session->opened && !session->closing && !session->suspended && !session->failed &&
      memory.U8(session->primary + 8) == 2 && !*enabled) {
    Call(__imp__sub_821D7670, session->selector, 1);
    if (!Guard(memory.U8(*child + 0x18) == 1, "settings_selector_enabled_result")) return false;
  }
  return true;
}

void Present(Event event) {
  if (!session) return;
  const auto view = CurrentView();
  if (!NativeSettingsRenderPublish(view)) {
    session->failed = true;
    session->closing = true;
    Guard(false, "settings_render_publish");
    return;
  }
  if (!SynchronizeSelection(NativeSettingsRenderSelection())) {
    session->failed = true;
    session->closing = true;
    return;
  }
  if (session->opened && !session->suspended)
    for (auto& message : events.Present(session->identity, session->locale, event, view))
      QueueGameEvent(std::move(message));
}

void Notice(const char* key) {
  if (!session) return;
  const auto found = session->catalog.text.find(key);
  if (found == session->catalog.text.end()) { Guard(false, "settings_notice_resource"); return; }
  if (session->notice == found->second) return;
  session->notice = found->second;
  Present(Event::NoticeChanged);
}

void SynchronizeHost() {
  if (!session || !host || session->host_command || menu.Read().pending ||
      host->CatalogRevision() == session->catalog_revision) return;
  const auto snapshot = host->ReadCatalogSnapshot();
  if (!snapshot) return;
  const auto& catalog = snapshot->catalog;
  const auto before = menu.Read();
  const std::vector<std::pair<Id, int32_t>> values(catalog.host_values.begin(), catalog.host_values.end());
  const std::vector<ChoiceUpdate> choices{
      {WindowSize, catalog.window_sizes}, {RenderScale, catalog.render_scales}};
  if (!Guard(menu.SynchronizeValues(values, choices) == Status::Ok, "settings_external_values")) return;
  session->catalog_revision = snapshot->revision;
  session->catalog.host_values = catalog.host_values;
  session->catalog.window_sizes = catalog.window_sizes;
  session->catalog.render_scales = catalog.render_scales;
  const auto after = menu.Read();
  const bool selected_changed = before.position && before.position <= before.rows.size() &&
      before.rows[before.position - 1].numeric_value != after.rows[after.position - 1].numeric_value;
  Present(selected_changed ? Event::ValueChanged : Event::None);
}

void PollHost() {
  if (!host) return;
  // Bounded work even if a backend mistakenly floods completion events.
  for (unsigned i = 0; i < 32; ++i) {
    const auto event = host->Poll();
    if (!event) break;
    if (!session) {
      if (event->kind == HostEventKind::SaveFailed) {
        pending_notices.emplace("save_failed");
        Guard(false, "settings_host_save_after_close");
      }
      if (event->kind == HostEventKind::ApplyFailed) {
        pending_notices.emplace("apply_failed");
        Guard(false, "settings_host_apply_after_close");
      }
      continue;
    }
    if (event->kind == HostEventKind::Committed || event->kind == HostEventKind::Rejected) {
      if (!session->host_command || *session->host_command != event->generation) continue;
      session->host_command.reset();
      const auto resolved = menu.Resolve(event->generation, event->kind == HostEventKind::Committed);
      if (resolved.status == Status::Rejected) Notice("apply_failed");
      else {
        Present(resolved.event);
        if (event->restart_required) Notice("restart_required");
      }
    } else if (event->kind == HostEventKind::SaveFailed) Notice("save_failed");
    else if (event->kind == HostEventKind::ApplyFailed) Notice("apply_failed");
  }
}

bool ReadNativeValues(std::array<uint8_t, 6>& values) {
  return session && Read(session->model + 0x24, values.data(), values.size());
}

bool ApplyNative(const Command& command) {
  if (!session || !session->saves || !session->saves->fault().empty() ||
      !OwnsModel(*session) || !AudioContext()) return false;
  std::array<uint8_t, 6> values{};
  if (!ReadNativeValues(values)) return false;
  const auto input = memory.U32BE(0x829CDA70);
  if (!input || !Object(*input, 0x82035B48, "settings_input_effect_owner")) return false;
  for (const auto& change : command.changes) {
    if (change.row < ThrowingPractice || change.row > Voice) return false;
    const auto row = static_cast<size_t>(change.row - ThrowingPractice);
    if (values[row + 1] != change.before || change.after < (row < 2 ? 0 : 1) ||
        change.after > (row < 2 ? 1 : 10)) return false;
  }
  for (const auto& change : command.changes) {
    const uint32_t row = static_cast<uint32_t>(change.row - ThrowingPractice);
    const uint32_t direction = change.after > change.before ? 1 : 0;
    int32_t value = change.before;
    while (value != change.after) {
      Call(__imp__sub_823341A0, session->primary, direction, row);
      value += direction ? 1 : -1;
      if (!Guard(memory.U8(session->model + 0x25 + row) == value, "settings_native_effect_result")) {
        session->failed = true;
        session->closing = true;
        return false;
      }
    }
  }
  if (!ReadNativeValues(values)) return false;
  const auto result = session->saves->Enqueue(command.generation, values);
  return Guard(result.status == SaveStatus::Queued, "settings_native_save_enqueue");
}

void Handle(Input input) {
  session->notice.clear();
  const auto result = menu.Handle(input);
  if (result.status != Status::Ok) return;
  if (result.command) {
    const auto& command = *result.command;
    const bool native = command.changes.empty() ||
        (command.changes.front().row >= ThrowingPractice && command.changes.front().row <= Voice);
    if (native) {
      const bool accepted = command.changes.empty() || ApplyNative(command);
      if (session->failed) { Notice("apply_failed"); return; }
      const auto resolved = menu.Resolve(command.generation, accepted);
      if (!accepted) Notice("apply_failed");
      else {
        if (command.kind == CommandKind::Adjust && command.row == Voice) PreviewVoice();
        else Sound(input == Input::Activate ? 0 : 5);
        Present(resolved.event);
      }
    } else if (host && host->Submit(command)) {
      session->host_command = command.generation;
      Sound(input == Input::Activate ? 0 : 5);
    } else {
      menu.Resolve(command.generation, false);
      Notice("apply_failed");
    }
    return;
  }
  if (result.event == Event::CloseRequested) {
    session->closing = true;
    Sound(1);
    return;
  }
  if (result.event != Event::None) {
    Sound(input == Input::Back ? 1 : input == Input::Activate ? 0 : 5);
    Present(result.event);
  }
}

std::optional<Input> ReadInput(uint32_t* held = nullptr) {
  if (held) *held = UINT32_MAX;
  const auto manager = memory.U32BE(0x829CDA70);
  if (!manager || !Object(*manager, 0x82035B48, "settings_input_owner")) return std::nullopt;
  const auto player = memory.U8(session->model + 0x1C);
  if (!Guard(player && *player < 4, "settings_input_player")) return std::nullopt;
  const auto cursors = memory.U32BE(0x829CED20);
  if (!Guard(cursors && *cursors && *cursors <= UINT32_MAX - 4 * 24, "settings_input_cursor_owner"))
    return std::nullopt;
  const auto enabled = memory.U8(*cursors + *player * 24 + 0x15);
  if (!Guard(enabled.has_value(), "settings_input_cursor_enabled") || !*enabled) return std::nullopt;
  const auto mapping = memory.U32BE(*manager + (0x1107 + *player) * 4);
  const auto container = memory.U32BE(0x829E7960);
  if (!Guard(mapping && container && *container, "settings_input_mapping")) return std::nullopt;
  const auto begin = memory.U32BE(*container + 0xC), end = memory.U32BE(*container + 0x10);
  if (!Guard(begin && end && *begin && *end >= *begin && (*end - *begin) % 4 == 0 &&
             (*mapping & 0xFF) < (*end - *begin) / 4,
             "settings_controller_vector")) return std::nullopt;
  const auto controller = memory.U32BE(*begin + (*mapping & 0xFF) * 4);
  if (!Guard(controller && *controller, "settings_controller_owner")) return std::nullopt;
  const auto connected = memory.U8(*controller + 0xC);
  if (!connected) return std::nullopt;
  if (!*connected) return std::nullopt;
  const auto holding = memory.U32BE(*controller + 0x14);
  if (!Guard(holding.has_value(), "settings_controller_held")) return std::nullopt;
  if (held) *held = *holding & 0xFF00C;
  const auto pressed = memory.U32BE(*controller + 0x18), repeated = memory.U32BE(*controller + 0x20);
  if (!Guard(pressed && repeated, "settings_controller_bits")) return std::nullopt;
  if (*pressed & 8) return Input::Back;
  if (*pressed & 4) return Input::Activate;
  if (*repeated & 0x11000) return Input::Up;
  if (*repeated & 0x22000) return Input::Down;
  if (*repeated & 0x44000) return Input::Left;
  if (*repeated & 0x88000) return Input::Right;
  return std::nullopt;
}

bool NativeChildFocused() {
  if (memory.U32BE(0x829CBC48).value_or(1) != 0) return true;
  const auto service = memory.U32BE(0x829CC350);
  if (!service || !*service) return true;
  // Silent option saves retain input during ordinary writing. Native error and
  // retry paths set the popup flag, including their separate save-slot child.
  const auto operation = memory.U32BE(*service + 0xC);
  const auto popup = memory.U8(*service + 0x28);
  return !operation || !popup || (*operation != 0 && *popup != 0);
}

bool Prepare(uint32_t model, PPCContext& ctx, uint8_t* base) {
  if (session || !guest_memory || !host || model < 0x20) return false;
  const auto snapshot = host->ReadCatalogSnapshot();
  if (!snapshot) return false;
  auto candidate = std::make_unique<Session>();
  candidate->model = model; candidate->task = model - 0x20;
  candidate->primary = memory.U32BE(model + 0x14).value_or(0);
  candidate->secondary = memory.U32BE(model + 0x18).value_or(0);
  candidate->selector = memory.U32BE(model + 0xC).value_or(0);
  candidate->common = memory.U32BE(0x829CBC68).value_or(0);
  if (!OwnsModel(*candidate) || !Object(candidate->common, 0x82003EAC, "settings_common_setup"))
    return false;
  const auto resource = memory.U16BE(candidate->common + 0x20);
  if (!Guard(resource && *resource >= 0x8CC && *resource <= 0x8D2, "settings_locale")) return false;
  candidate->locale = *resource - 0x8CB;
  candidate->catalog = snapshot->catalog;
  candidate->catalog_revision = snapshot->revision;
  const auto title = brawlers::ui::ReadMenuText(memory, 0x612);
  if (!Guard(title.value.has_value(), "settings_title_text")) return false;
  candidate->catalog.title = *title.value;
  constexpr std::array<uint32_t, 5> labels{0x928, 0xA0D, 0x7BE, 0x7BF, 0x7C0};
  for (size_t i = 0; i < labels.size(); ++i) {
    const auto label = brawlers::ui::ReadMenuText(memory, labels[i]);
    if (!Guard(label.value.has_value(), "settings_native_label_text")) return false;
    candidate->catalog.native_labels[i] = *label.value;
  }
  if (!Read(model + 0x24, candidate->catalog.native_values.data(), 6)) return false;
  const auto definition = BuildDefinition(candidate->catalog);
  if (!Guard(definition && menu.Configure(*definition), "settings_definition")) return false;
  candidate->storage = guest_memory->SystemHeapAlloc(16);
  if (!Guard(candidate->storage != 0, "settings_save_allocation")) return false;
  candidate->save_lifetime = std::make_shared<SaveLifetime>();
  candidate->save_lifetime->common = candidate->common;
  candidate->saves = std::make_unique<NativeSettingsSave>(SaveAccess{Read, Write, SaveCallOriginal,
      [lifetime = candidate->save_lifetime] { return SaveContext(*lifetime); }});
  if (!candidate->saves->Init(candidate->storage)) {
    guest_memory->SystemHeapFree(candidate->storage);
    return false;
  }
  if (!NativeSettingsRenderBegin(model, ctx, base)) {
    guest_memory->SystemHeapFree(candidate->storage);
    return false;
  }
  candidate->identity = ++next_identity;
  session = std::move(candidate);
  menu.Open();
  return true;
}

void Finish(PPCContext& ctx, uint8_t* base) {
  if (!session) return;
  for (auto& event : events.Closed()) QueueGameEvent(std::move(event));
  NativeSettingsRenderEnd(session->model, ctx, base);
  menu.Close();
  if (session->saves->OwnerClosed()) {
    pending_notices.emplace("save_failed");
    Guard(false, "settings_save_queued_owner_closed");
  }
  if (session->saves->Idle() || session->saves->CanRelease())
    guest_memory->SystemHeapFree(session->storage);
  else {
    session->save_lifetime->detached = true;
    retained_saves.push_back({session->storage, session->save_lifetime, std::move(session->saves)});
    Guard(false, "settings_save_owner_retained");
  }
  session.reset();
}

}  // namespace

void SetNativeSettings(rex::memory::Memory* value, HostSettingsRuntime* settings_host) {
  guest_memory = value; host = settings_host;
  memory = GuestUiMemory(Read);
  SetNativeSettingsRenderAccess(Read, Write);
}

void UpdateHostSettings(PPCContext& ctx, uint8_t* base) {
  GuestCallScope calls(ctx, base);
  if (!session && host && host->HasPendingWork()) PollHost();
  for (auto it = retained_saves.begin(); it != retained_saves.end();) {
    if (it->saves->OwnerClosed()) pending_notices.emplace("save_failed");
    if (!it->saves->fault().empty()) it->saves->RetryValidation();
    const auto saved = it->saves->Pump();
    if (saved.status == SaveStatus::Fault || saved.status == SaveStatus::Abandoned)
      pending_notices.emplace("save_failed");
    else if (saved.status == SaveStatus::RamOnly) pending_notices.emplace("save_unavailable");
    if (it->saves->Idle() || it->saves->CanRelease()) {
      guest_memory->SystemHeapFree(it->storage);
      it = retained_saves.erase(it);
    } else ++it;
  }
}

extern "C" void sub_82335320(PPCContext& ctx, uint8_t* base) {
  GuestCallScope calls(ctx, base);
  const uint32_t model = ctx.r3.u32;
  const bool managed = Prepare(model, ctx, base);
  __imp__sub_82335320(ctx, base);
  if (managed) {
    if (!NativeSettingsRenderBuilt(model, ctx, base)) {
      session->failed = true;
      session->closing = true;
      Guard(false, "settings_render_build");
    } else Present(Event::None);
  }
}

extern "C" void sub_823351E8(PPCContext& ctx, uint8_t* base) {
  if (!session || ctx.r3.u32 != session->model) { __imp__sub_823351E8(ctx, base); return; }
  GuestCallScope calls(ctx, base);
  ctx.r3.u64 = 0;
  if (!OwnsModel(*session)) return;
  if (!NativeSettingsRenderHealthy()) {
    session->failed = true;
    session->closing = true;
  }
  const bool was_suspended = session->suspended;
  session->suspended = NativeChildFocused();
  NativeSettingsRenderSuspend(session->suspended);
  PollHost();
  SynchronizeHost();
  const auto saved = session->saves->Pump();
  session->suspended = NativeChildFocused();
  NativeSettingsRenderSuspend(session->suspended);
  if (saved.status == SaveStatus::Fault) {
    Guard(false, saved.reason.empty() ? "settings_save_fault" : saved.reason.data());
    session->saves->RetryValidation();
    session->saves->AbandonQueued();
    if (!session->save_fault_reported) {
      session->save_fault_reported = true;
      Notice("save_failed");
    }
  } else if (saved.status == SaveStatus::Abandoned) Notice("save_failed");
  else if (saved.status == SaveStatus::RamOnly) Notice("save_unavailable");
  const bool suspended = session->suspended;
  if (suspended != was_suspended) {
    session->suspended = suspended;
    NativeSettingsRenderSuspend(suspended);
    if (!suspended) {
      session->quarantine = true;
      Present(Event::Returned);
      return;
    }
  }
  if (suspended) return;
  const auto phase = memory.U8(session->primary + 8);
  if (!phase) return;
  if (session->closing && !session->host_command && session->saves->Idle()) {
    const auto service = memory.U32BE(0x829CC350);
    if (!service || memory.U32BE(*service + 0xC) != 0 || memory.U32BE(*service + 0x24) != 0) return;
    if (*phase == 2) {
      StopVoice();
      Byte(session->model + 0x30, 6);
      Byte(session->primary + 8, 6);
    }
    ctx.r3.u64 = Call(__imp__sub_82334888, session->model);
    return;
  }
  if (*phase != 2 || session->failed) return;
  if (!session->opened) {
    session->opened = true;
    for (const auto& key : pending_notices) {
      const auto text = session->catalog.text.find(key);
      if (text != session->catalog.text.end()) {
        if (!session->notice.empty()) session->notice += " ";
        session->notice += text->second;
      }
    }
    pending_notices.clear();
    Present(Event::Opened);
    return;
  }
  uint32_t held = 0;
  const auto input = ReadInput(&held);
  if (session->quarantine) {
    if (!held) session->quarantine = false;
    return;
  }
  if (input) Handle(*input);
}

extern "C" void sub_82334E50(PPCContext& ctx, uint8_t* base) {
  const bool managed = session && session->model == ctx.r3.u32;
  // Initialization failure accepted no edits. Preserve native closing and
  // cleanup without creating a save merely because the renderer could not open.
  // A rejected pre-request save must also avoid the unchecked final helper.
  const bool previous_skip = skip_final_save;
  const bool untouched_failure = managed && session->failed &&
      !session->opened && !session->host_command && session->saves->Idle();
  skip_final_save = untouched_failure ||
      (managed && !session->saves->fault().empty() && session->saves->CanRelease());
  if (skip_final_save)
    skipped_final_saves[session->task] = memory.U32BE(0x829CC350).value_or(0);
  __imp__sub_82334E50(ctx, base);
  skip_final_save = previous_skip;
  if (managed) { GuestCallScope calls(ctx, base); Finish(ctx, base); }
}

extern "C" void sub_822E6288(PPCContext& ctx, uint8_t* base) {
  if (!skip_final_save) __imp__sub_822E6288(ctx, base);
}

extern "C" void sub_82333CD0(PPCContext& ctx, uint8_t* base) {
  const auto previous = updating_task;
  updating_task = ctx.r3.u32;
  __imp__sub_82333CD0(ctx, base);
  updating_task = previous;
}

extern "C" void sub_822E6560(PPCContext& ctx, uint8_t* base) {
  const auto skipped = skipped_final_saves.find(updating_task);
  if (skipped == skipped_final_saves.end()) { __imp__sub_822E6560(ctx, base); return; }
  // A rejected final request has nothing to pump. The original idle pump still
  // dereferences its common UI context, which may be the rejected owner. Only
  // this task's state-five wait may complete without that unsafe native call.
  const auto service = skipped->second;
  const bool released = Object(updating_task, 0x82019C40, "settings_skipped_save_task") &&
      memory.U32BE(updating_task + 0x14) == 5 && service &&
      memory.U32BE(0x829CC350) == service && memory.U32BE(service) == 0 &&
      memory.U32BE(service + 0xC) == 0 && memory.U32BE(service + 0x24) == 0 &&
      memory.U32BE(0x829CA614) == 0 && memory.U32BE(0x829CA618) == 0;
  ctx.r3.u64 = released ? 1 : 0;
  if (released) skipped_final_saves.erase(skipped);
  else Guard(false, "settings_skipped_save_not_released");
}

extern "C" void sub_82333E38(PPCContext& ctx, uint8_t* base) {
  skipped_final_saves.erase(ctx.r3.u32);
  if (session && session->task == ctx.r3.u32) {
    GuestCallScope calls(ctx, base);
    Finish(ctx, base);
  }
  __imp__sub_82333E38(ctx, base);
}
