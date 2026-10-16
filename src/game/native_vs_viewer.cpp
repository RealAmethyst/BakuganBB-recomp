// Reads only the text actually drawn by the VS deck's Bakugan detail viewer.
#include "native_vs_viewer.h"
#include "native_vs_editor.h"
#include "native_localization.h"

#include <cfenv>
#include <map>
#include <set>
#include <utility>

#include <rex/ppc/context.h>
#include <rex/thread/mutex.h>

extern "C" void __imp__sub_8230E9F8(PPCContext&, uint8_t*);
extern "C" void __imp__sub_822DC058(PPCContext&, uint8_t*);
extern "C" void __imp__sub_822DD1E0(PPCContext&, uint8_t*);

namespace {
using brawlers::ui::GuestUiMemory;
GuestUiMemory memory({});
rex::thread::global_critical_region state_region;
std::function<void(ScreenEvent&&)> output;
std::function<void(std::string_view)> diagnostic;
std::function<void()> failure;
std::set<std::string> reported;
uint64_t next_identity = 0;
bool failed = false;

struct Binding {
  uint32_t root = 0, summary = 0, editor = 0, manager = 0;
};
thread_local Binding preview;
struct Viewer {
  Binding binding;
  uint64_t id = 0;
  uint32_t controller = 0;
  bool active = false, opened = false;
  bool have_name = false, have_description = false;
  std::string name, description;
};
std::map<uint32_t, Viewer> viewers;

class FloatEnvironment {
 public:
  FloatEnvironment() : valid_(std::fegetenv(&value_) == 0) {}
  ~FloatEnvironment() { if (valid_) std::fesetenv(&value_); }
 private:
  std::fenv_t value_;
  bool valid_;
};

template <typename Function>
void Observe(Function function) noexcept {
  FloatEnvironment preserve;
  try {
    auto lock = state_region.Acquire();
    if (failed) return;
    try { function(); }
    catch (...) {
      failed = true;
      if (failure) failure();
    }
  } catch (...) {
    // Never replace a failed runtime lock with unprotected guest reads.
  }
}

bool Guard(bool valid, const char* reason) {
  if (!valid && diagnostic && reported.emplace(reason).second) diagnostic(reason);
  return valid;
}

bool Parents(const Binding& binding) {
  return Guard(binding.root && binding.root <= UINT32_MAX - 0x440 &&
               memory.U32BE(0x829CC54C) == binding.root &&
               memory.U32BE(binding.root) == 0x8201A078 &&
               memory.U32BE(binding.root + 0x14) == 8, "vs_viewer_root") &&
         Guard(binding.summary && binding.summary <= UINT32_MAX - 0x208 &&
               memory.U32BE(binding.summary) == 0x82017DDC &&
               memory.U32BE(binding.summary + 0x14) == 6 &&
               memory.U32BE(binding.summary + 0x18) == binding.root + 0x42C &&
               memory.U32BE(binding.summary + 0x1F4) == binding.root + 0x44,
               "vs_viewer_summary") &&
         Guard(binding.editor && binding.editor <= UINT32_MAX - 0x4E0 &&
               memory.U32BE(binding.editor) == 0x82018DE8 &&
               memory.U32BE(binding.editor + 0x18) == 0 &&
               memory.U32BE(binding.editor + 0x2C) == binding.summary + 0x200,
               "vs_viewer_editor") &&
         Guard(binding.manager && binding.manager <= UINT32_MAX - 0x30 &&
               memory.U32BE(0x829CC330) == binding.manager &&
               memory.U32BE(binding.manager) == 0x82003F2C, "vs_viewer_manager");
}

bool Owner(uint32_t owner, const Viewer& viewer) {
  return Guard(owner && owner <= UINT32_MAX - 0xB4 &&
               memory.U32BE(owner) == 0x82017294 &&
               memory.U32BE(owner + 0x20) == 0x82017278 &&
               memory.U32BE(owner + 0x18) == viewer.binding.manager + 0x2C,
               "vs_viewer_owner") && Parents(viewer.binding);
}

ScreenEvent Event(const Viewer& viewer, const char* kind) {
  ScreenEvent event;
  event.source = "vs_viewer";
  event.kind = kind;
  event.id = viewer.id;
  event.controller = viewer.controller;
  return event;
}

void Emit(uint32_t owner, Viewer& viewer) {
  if (!viewer.active || viewer.opened || !viewer.have_name || !viewer.have_description ||
      !output || !Owner(owner, viewer)) return;
  if (!Guard(memory.U32BE(owner + 0x14) == 3 &&
             memory.U32BE(viewer.binding.editor + 0x14) == 0x10,
             "vs_viewer_active_state")) return;
  const auto hint = brawlers::ui::ReadMenuText(memory, 0xA08);
  if (!Guard(hint.value.has_value(), hint.reason)) return;
  auto event = Event(viewer, "opened");
  // The upper name is content; this viewer has no separate displayed title.
  event.body = viewer.name;
  if (!viewer.description.empty()) event.body += "\n" + viewer.description;
  event.hints.push_back(*hint.value);
  output(std::move(event));
  viewer.opened = true;
}

void Text(uint32_t model, uint32_t id, bool description) {
  if (model < 0x20) return;
  const auto found = viewers.find(model - 0x20);
  if (found == viewers.end() || found->second.opened || !found->second.active) return;
  auto& viewer = found->second;
  if (!Owner(found->first, viewer) ||
      !Guard(memory.U32BE(found->first + 0x14) == 3, "vs_viewer_text_state")) return;
  // These IDs come directly from r5 at the actual text draw. In particular,
  // biography lookup must not independently index an unbounded species table.
  const auto text = brawlers::ui::ReadMenuText(memory, id, description);
  if (!Guard(text.value.has_value(), text.reason)) return;
  if (description) {
    viewer.description = *text.value;
    viewer.have_description = true;
  } else {
    if (!Guard(!text.value->empty(), "vs_viewer_empty_name")) return;
    viewer.name = *text.value;
    viewer.have_name = true;
  }
  Emit(found->first, viewer);
}

void Close(uint32_t owner) {
  const auto found = viewers.find(owner);
  if (found == viewers.end()) return;
  if (found->second.opened && output) output(Event(found->second, "closed"));
  viewers.erase(found);
}
}  // namespace

void SetVsViewerCapture(brawlers::ui::ReadBytes reader,
                        std::function<void(ScreenEvent&&)> sink,
                        std::function<void(std::string_view)> report,
                        std::function<void()> fail) {
  memory = GuestUiMemory(std::move(reader));
  output = std::move(sink);
  diagnostic = std::move(report);
  failure = std::move(fail);
}

void NotifyVsViewerTaskRemoved(uint32_t owner) noexcept {
  Observe([&] { Close(owner); });
}

extern "C" void sub_8230E9F8(PPCContext& ctx, uint8_t* base) {
  struct Restore {
    Binding saved = preview;
    ~Restore() { preview = saved; }
  } restore;
  preview = {};
  const auto editor = ctx.r3.u32;
  Observe([&] {
    if (!output || !editor || editor > UINT32_MAX - 0x4E0) return;
    const auto completion = memory.U32BE(editor + 0x2C);
    const auto root = memory.U32BE(0x829CC54C);
    const auto manager = memory.U32BE(0x829CC330);
    if (!completion || *completion < 0x200 || !root || !manager) return;
    Binding binding{*root, *completion - 0x200, editor, *manager};
    if (!Parents(binding)) return;
    const auto state = memory.U32BE(editor + 0x14);
    const auto section = memory.U8(editor + 0x3A);
    const auto participant = memory.U32BE(editor + 0x1C);
    if (!Guard(state && (*state == 5 || *state == 8) && section &&
               (*section == 2 || *section == 4), "vs_viewer_preview_section") ||
        !Guard(participant && *participant >= *root + 0x44 &&
               *participant < *root + 0x424 &&
               (*participant - *root - 0x44) % 0xF8 == 0,
               "vs_viewer_participant")) return;
    preview = binding;
  });
  __imp__sub_8230E9F8(ctx, base);
  NotifyVsEditorViewerOpened(editor);
}

extern "C" void sub_822DC058(PPCContext& ctx, uint8_t* base) {
  const auto owner = ctx.r3.u32, completion = ctx.r4.u32;
  const auto controller = ctx.r7.u32;
  const auto binding = preview;
  __imp__sub_822DC058(ctx, base);
  if (!binding.editor) return;
  Observe([&] {
    Viewer viewer;
    viewer.binding = binding;
    if (!Owner(owner, viewer) ||
        !Guard(completion == binding.manager + 0x2C, "vs_viewer_completion") ||
        !Guard(controller < 4 && memory.U8(owner + 0xB0) == controller &&
               memory.U8(binding.editor + 0x38) == controller, "vs_viewer_controller") ||
        !Guard(!viewers.contains(owner), "vs_viewer_duplicate_lifetime")) return;
    const auto resource = memory.U16BE(binding.manager + 0x20);
    if (!Guard(resource && *resource >= 0x9D4 && *resource <= 0x9DA,
               "vs_viewer_resource")) return;
    viewer.id = ++next_identity;
    viewer.controller = controller;
    viewers.emplace(owner, std::move(viewer));
  });
}

extern "C" void sub_822DD1E0(PPCContext& ctx, uint8_t* base) {
  Observe([&] { Close(ctx.r3.u32); });
  __imp__sub_822DD1E0(ctx, base);
}

void BrawlersVsViewerReady(PPCRegister& owner) {
  Observe([&] {
    const auto found = viewers.find(owner.u32);
    if (found != viewers.end()) found->second.active = true;
  });
}

void BrawlersVsViewerSuspended(PPCRegister& owner) {
  Observe([&] {
    const auto found = viewers.find(owner.u32);
    if (found != viewers.end()) found->second.active = false;
  });
}

void BrawlersVsViewerName(PPCRegister& model, PPCRegister& id) {
  Observe([&] { Text(model.u32, id.u32, false); });
}

void BrawlersVsViewerDescription(PPCRegister& model, PPCRegister& id) {
  Observe([&] { Text(model.u32, id.u32, true); });
}
