// Original synthetic fixtures; no game or accessibility services are used.
#include "native_vs_editor.h"

#include <cassert>
#include <cfenv>
#include <cstring>
#include <map>
#include <stdexcept>
#include <vector>

#include <rex/ppc/context.h>
#include <rex/thread/mutex.h>

extern "C" void sub_823111E0(PPCContext&, uint8_t*);
extern "C" void sub_82311528(PPCContext&, uint8_t*);
extern "C" void sub_8231CDA8(PPCContext&, uint8_t*);
extern "C" void sub_8230FDA0(PPCContext&, uint8_t*);
extern "C" void sub_8230F290(PPCContext&, uint8_t*);
extern "C" void sub_82305978(PPCContext&, uint8_t*);
extern "C" void sub_82312E18(PPCContext&, uint8_t*);
extern "C" void sub_8230F220(PPCContext&, uint8_t*);
extern "C" void sub_8231A578(PPCContext&, uint8_t*);
extern "C" void sub_823B9220(PPCContext&, uint8_t*);
void BrawlersVsEditorReady(PPCRegister&);
void BrawlersVsEditorCaption(PPCRegister&, PPCRegister&);
void BrawlersVsEditorHeader(PPCRegister&, PPCRegister&, PPCRegister&);
void BrawlersVsEditorHints(PPCRegister&, PPCRegister&);
void BrawlersVsEditorFooter(PPCRegister&, PPCRegister&, PPCRegister&);
void BrawlersVsEditorPreviewHint(PPCRegister&, PPCRegister&);
void BrawlersVsEditorStatus(PPCRegister&, PPCRegister&, PPCRegister&);
void BrawlersVsEditorPickerReady(PPCRegister&);
void BrawlersVsEditorPickerClosed(PPCRegister&);
void BrawlersVsEditorPickerTitle(PPCRegister&, PPCRegister&);
void BrawlersVsEditorConfirmationReady(PPCRegister&);
void BrawlersVsEditorConfirmationPrompt(PPCRegister&, PPCRegister&);
void BrawlersVsEditorConfirmationStatus(PPCRegister&, PPCRegister&, PPCRegister&);

std::recursive_mutex& rex::thread::global_critical_region::mutex() {
  static std::recursive_mutex lock;
  return lock;
}
namespace {
using Kind = VsEditorEventKind;
constexpr uint32_t task = 0x1000, summary = 0x2000, root = 0x3000, loader = 0x4000;
constexpr uint32_t flags = 0x5000, picker_flags = 0x5100;
constexpr uint32_t widget = 0x6000, header = 0x6200, hint = 0x6400, status = 0x6600;
constexpr uint32_t context = 0x7000, list = 0x7100, entries = 0x7200, hint_text = 0x7400;
constexpr uint32_t provider = 0x8000, bank = 0x9000, resource = 0x10000;
std::map<uint32_t, uint8_t> bytes;
std::vector<VsEditorEvent> events;
std::vector<std::string> guards;
unsigned originals = 0, reads = 0, failures = 0;
bool throw_output = false;
uint8_t opaque[1]{};
void Word(uint32_t address, uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) bytes[address + i] = value >> (24 - i * 8);
}
void Unit(uint32_t address, uint16_t value) {
  bytes[address] = value >> 8;
  bytes[address + 1] = value;
}
void String(uint32_t address, const char16_t* text) {
  do { Unit(address, *text); address += 2; } while (*text++);
}
void Text(uint32_t id, const char16_t* text) {
  String(resource + 0x10000 + id * 0x100, text);
}
void Graph(uint32_t selector, uint32_t count, uint32_t enabled, uint32_t selected) {
  Word(selector, 0x8200FC30);
  Word(selector + 0x28, selector + 0x40);
  Word(selector + 0x40, 0x8200FC10);
  Word(selector + 0x44, count == 9 ? 0x82018A98 : 0x82018814);
  bytes[selector + 0x58] = 1;
  Word(selector + 0x48, enabled);
  Word(selector + 0x50, selected);
  Word(selector + 0x54, count);
  for (uint32_t i = 0; i < count; ++i) bytes[enabled + i] = 1;
}
void Binding(uint32_t native_widget, uint32_t model) {
  Unit(native_widget + 0xA, 0);
  Word(context + 0xC, list);
  Word(list + 0xC, entries);
  Word(list + 0x10, entries + 4);
  Word(entries, model);
}
void Fixture() {
  Word(task, 0x82018DE8);
  Word(task + 0x14, 3);
  Word(task + 0x18, 0);
  Word(task + 0x1C, root + 0x44);
  Word(task + 0x2C, summary + 0x200);
  Word(task + 0x20, 0xA000);
  Word(task + 0xF4, 0xA000);
  Unit(root + 0x44 + 8, 432);
  bytes[root + 0x44 + 0x10] = 7;
  Unit(0xA000 + (2 * 6 + 2) * 0x14 + 8, 987);
  bytes[0xA000 + (2 * 6 + 2) * 0x14 + 0x10] = 10;
  bytes[task + 0x38] = 0;
  bytes[task + 0x3A] = 0;
  bytes[task + 0x3C] = 0;
  bytes[task + 0x44] = 0;
  Word(task + 0x50, 1);
  Word(task + 0xA0, loader);
  Word(loader, 0x82003F2C);
  Unit(loader + 0x20, 0x9B4);
  for (uint32_t offset : {0xA8U, 0xC4U, 0xE0U}) {
    Word(task + offset, 0x82018C1C);
    Word(task + offset + 0xC, task + 0x148);
    Word(task + offset + 0x10, root + 0x44);
    Word(task + offset + 0x18, task + 0x38);
  }
  Graph(task + 0x148, 9, flags, 8);
  bytes[flags] = bytes[flags + 7] = 0;
  bytes[flags + 4] = bytes[flags + 5] = bytes[flags + 6] = 0;
  for (uint32_t slot = 0; slot < 6; ++slot) {
    bytes[task + 0x5B + slot] = 0;
    Unit(task + 0x62 + slot * 2, slot + 2);
    bytes[task + 0x6E + slot] = 2;
    bytes[task + 0x74 + slot] = 1;
  }
  Word(task + 0xFC, 0x82018C20);
  Word(task + 0x108, task + 0x1A8);
  Word(task + 0x1A8, 0x82003900);
  Word(task + 0x1D0, task + 0x1E8);
  Word(task + 0x1E8, 0x820038DC);
  Word(task + 0x1EC, 0);
  Word(task + 0x1F0, 0);
  Word(task + 0x1F4, 2);
  bytes[task + 0x204] = 0;
  Graph(task + 0x210, 6, picker_flags, 0);
  Word(task + 0x274, 0);
  Word(task + 0x278, 1);
  Word(task + 0x298, 0x82018880);
  Word(task + 0x2A4, task + 0x210);
  Word(task + 0x2A8, task + 0x274);
  Word(summary, 0x82017DDC);
  Word(summary + 0x14, 5);
  Word(summary + 0x18, root + 0x42C);
  Word(summary + 0x1F4, root + 0x44);
  Word(summary + 0x1FC, 1);
  Word(0x829CC54C, root);
  Word(root, 0x8201A078);
  Word(root + 0x14, 8);
  Word(widget, 0x82018D88);
  Word(header, 0x82018D9C);
  Word(hint, 0x82018D94);
  String(widget + 0x104, u"Synthetic deck");
  String(root + 0x44 + 0xA8, u"Synthetic deck");
  String(hint_text, u"Synthetic native hints");
  Word(0x829CEB78, provider);
  Word(provider, 0x82036508);
  Word(provider + 8, bank);
  bytes[provider + 0xC] = 1;
  bytes[bank + 0x60] = 1;
  Word(bank, resource);
  Word(bank + 4, resource + 0x4000);
  Word(bank + 8, resource + 0x20);
  Word(resource, 0x70617374);
  Word(resource + 4, 0x7274626C);
  bytes[resource + 0xB] = 1;
  Word(resource + 0xC, 0xB00);
  Word(resource + 0x10, 0x20);
  Word(resource + 0x14, 0x4000);
  for (uint32_t id = 0; id < 0xB00; ++id)
    Word(resource + 0x20 + id * 4, 0x10000 + id * 0x100);
  for (uint32_t id = 0x656; id <= 0x65B; ++id) Text(id, u"Synthetic attribute");
  Text(0x6B9, u"Synthetic attribute title");
  Text(0x6BC, u"Synthetic color title");
  Text(0x98B, u"Synthetic attribute description");
  for (uint32_t id = 0x563; id < 0x569; ++id) Text(id, u"Synthetic color description");
  Text(0x914, u"Synthetic confirmation prompt");
  Text(0x915, u"Synthetic success status");
  Text(0x9D6, u"Synthetic common footer");
  Text(0x5E0, u"Synthetic native preview hint");
  Text(0x996, u"Synthetic locked deck status");
  Text(0x51A, u"Synthetic empty inventory status");
}
void Original(PPCContext& ctx, uint8_t* base) {
  assert(base == opaque);
  ++originals;
  ctx.r3.u64 = 0xABCDEF;
  ctx.r7.u64 = 0xABCDEF0123456789;
  ctx.f1.f64 = 0.5;
  std::fesetround(FE_DOWNWARD);
  std::feclearexcept(FE_ALL_EXCEPT);
  std::feraiseexcept(FE_INVALID);
}
void Call(void (*hook)(PPCContext&, uint8_t*), uint32_t owner = task, bool valid = true) {
  PPCContext ctx{};
  ctx.r3.u32 = owner;
  ctx.r4.u32 = hook == sub_8231A578 ? context : summary + 0x200;
  ctx.r5.u32 = valid ? root + 0x20 : 0;
  ctx.r6.u32 = root + 0x44;
  ctx.r7.u32 = 0;
  ctx.r8.u32 = 1;
  ctx.f2.f64 = 0.125;
  ctx.lr = 0x12345678;
  const auto before = originals;
  hook(ctx, opaque);
  assert(originals == before + 1);
  assert(ctx.r3.u64 == 0xABCDEF && ctx.r7.u64 == 0xABCDEF0123456789);
  assert(ctx.r8.u32 == 1 && ctx.f1.f64 == 0.5 && ctx.f2.f64 == 0.125);
  assert(ctx.lr == 0x12345678);
  assert(std::fegetround() == FE_DOWNWARD);
  assert(std::fetestexcept(FE_ALL_EXCEPT) == FE_INVALID);
}
template <typename Hook, typename... Values>
void Invoke(Hook hook, Values... values) {
  PPCRegister regs[sizeof...(values)]{};
  uint32_t input[]{static_cast<uint32_t>(values)...};
  for (size_t i = 0; i < sizeof...(values); ++i) regs[i].u32 = input[i];
  std::fesetround(FE_UPWARD);
  std::feclearexcept(FE_ALL_EXCEPT);
  std::feraiseexcept(FE_DIVBYZERO);
  [&]<size_t... I>(std::index_sequence<I...>) { hook(regs[I]...); }(std::index_sequence_for<Values...>{});
  for (size_t i = 0; i < sizeof...(values); ++i) assert(regs[i].u32 == input[i]);
  assert(std::fegetround() == FE_UPWARD);
  assert(std::fetestexcept(FE_ALL_EXCEPT) == FE_DIVBYZERO);
}
void Select(uint32_t row) {
  Word(task + 0x198, row);
  NotifyVsEditorSelection(task + 0x148, row);
  if (row >= 1 && row <= 6) bytes[task + 0x3C] = row - 1;
  Call(sub_8230FDA0);
}
void Caption(const char16_t* text) {
  String(widget + 0x104, text);
  Invoke(BrawlersVsEditorCaption, widget, task + 0xC4);
}
}  // namespace
extern "C" void __imp__sub_823111E0(PPCContext& c, uint8_t* b) { Original(c, b); }
extern "C" void __imp__sub_82311528(PPCContext& c, uint8_t* b) { Original(c, b); }
extern "C" void __imp__sub_8231CDA8(PPCContext& c, uint8_t* b) { Original(c, b); }
extern "C" void __imp__sub_8230FDA0(PPCContext& c, uint8_t* b) { Original(c, b); }
extern "C" void __imp__sub_8230F290(PPCContext& c, uint8_t* b) { Original(c, b); }
extern "C" void __imp__sub_82305978(PPCContext& c, uint8_t* b) { Original(c, b); }
extern "C" void __imp__sub_82312E18(PPCContext& c, uint8_t* b) { Original(c, b); }
extern "C" void __imp__sub_8230F220(PPCContext& c, uint8_t* b) { Original(c, b); }
extern "C" void __imp__sub_8231A578(PPCContext& c, uint8_t* b) {
  PPCRegister w{}, t{};
  w.u32 = hint; t.u32 = hint_text;
  BrawlersVsEditorHints(w, t);
  Original(c, b);
}
extern "C" void __imp__sub_823B9220(PPCContext& c, uint8_t* b) { Original(c, b); }

int main() {
  Fixture();
  SetVsEditorCapture([](uint32_t address, void* result, size_t length) {
    ++reads;
    std::fesetround(FE_TOWARDZERO);
    std::feraiseexcept(FE_OVERFLOW);
    for (size_t i = 0; i < length; ++i) {
      const auto found = bytes.find(address + static_cast<uint32_t>(i));
      if (found == bytes.end()) return false;
      static_cast<uint8_t*>(result)[i] = found->second;
    }
    return true;
  }, [](VsEditorEvent&& event) {
    if (throw_output) throw std::runtime_error("synthetic callback failure");
    events.push_back(std::move(event));
  }, [](std::string_view reason) { guards.emplace_back(reason); }, [] { ++failures; });
  const auto idle = reads;
  NotifyVsEditorSelection(0xDEAD, 0);
  Invoke(BrawlersVsEditorCaption, widget, 0xDEAD);
  assert(reads == idle && events.empty());
  Call(sub_823111E0, task, false);
  Invoke(BrawlersVsEditorReady, task);
  assert(events.empty());
  Call(sub_823111E0);
  Word(summary + 0x14, 6);
  Invoke(BrawlersVsEditorReady, task);
  assert(events.size() == 2 && events[0].kind == Kind::Opened);
  assert(events[1].selection->finish && events[1].selection->row == 8);
  const auto identity = events[0].id;
  Word(0x829CBC68, 0xB000);
  Word(0xB000, 0x82003EAC);
  Word(0xB0C4, 0x9D6);
  Word(0xB0CC, UINT32_MAX);
  Word(0xB0D4, UINT32_MAX);
  Binding(status, 0xB094);
  Invoke(BrawlersVsEditorFooter, status, context, 0xB094);
  assert(events.back().kind == Kind::Footer && events.back().text == "Synthetic common footer");
  events.pop_back();
  Select(1);
  assert(events.size() == 2);
  Caption(u"Selected synthetic deck");
  assert(events.back().selection->caption == "Selected synthetic deck");
  assert(events.back().selection->section == 0);
  const auto selected = events.size();
  Select(1);
  Caption(u"Selected synthetic deck");
  assert(events.size() == selected);
  const auto steady = reads;
  Caption(u"Ignored stale caption");
  assert(reads == steady);
  NotifyVsEditorSelection(task + 0x148, 2);
  assert(events.size() == selected);
  bytes[task + 0x3A] = 2;
  Word(task + 0x14, 5);
  Call(sub_8230F290);
  Invoke(BrawlersVsEditorReady, task);
  Caption(u"Synthetic creature");
  assert(events.back().selection->item_id == 2 && events.back().selection->attribute == 2);
  assert(events.back().selection->displayed_number == 432);
  assert(events.back().selection->level == 7);
  Invoke(BrawlersVsEditorHeader, header, task + 0xC4, root + 0x44 + 0xA8);
  assert(events.back().kind == Kind::Header && events.back().text == "Synthetic deck");
  Binding(hint, task + 0xA8);
  Call(sub_8231A578);
  assert(events.back().kind == Kind::Hints && events.back().text == "Synthetic native hints");
  const auto hints_read = reads;
  Binding(hint, task + 0xA8);
  Call(sub_8231A578);
  assert(events.back().kind == Kind::Hints && reads == hints_read);
  Call(sub_8230F290);
  Word(status, 0x82018DAC);
  Binding(status, task + 0xC4);
  Invoke(BrawlersVsEditorPreviewHint, status, context);
  assert(events.back().kind == Kind::Hints && events.back().text == "Synthetic native preview hint");
  Unit(root + 0x44 + 8, 0xFFFF);
  Caption(u"Synthetic creature");
  assert(events.back().selection->displayed_number == 0);
  Unit(root + 0x44 + 8, 1000);
  Call(sub_8230F290);
  Caption(u"Synthetic creature");
  assert(!events.back().selection->displayed_number);
  Unit(root + 0x44 + 8, 432);
  Call(sub_8230F290);
  Caption(u"Synthetic creature");
  Word(task + 0x14, 16);
  NotifyVsEditorViewerOpened(task);
  assert(events.back().kind == Kind::Suspended);
  const auto viewer = events.size();
  Invoke(BrawlersVsEditorReady, task);
  Caption(u"Suppressed viewer parent");
  assert(events.size() == viewer);
  Word(task + 0x14, 5);
  Invoke(BrawlersVsEditorReady, task);
  Caption(u"Synthetic creature");
  assert(events.back().kind == Kind::Resumed && events.back().id == identity);

  Word(task + 0x14, 6);
  Call(sub_82305978, task + 0x20C);
  Word(task + 0x14, 7);
  bytes[provider + 0xC] = 0;
  const auto delayed_picker = events.size();
  Invoke(BrawlersVsEditorPickerReady, task + 0x20C);
  assert(events.size() == delayed_picker);
  bytes[provider + 0xC] = 1;
  Word(status, 0x82018950);
  Binding(status, task + 0x298);
  Invoke(BrawlersVsEditorPickerTitle, status, context);
  assert(events[events.size() - 2].kind == Kind::PickerOpened);
  assert(events.back().kind == Kind::PickerFocused && events.back().choices == 6);
  assert(events.back().text == "Synthetic attribute description");
  const auto picker_id = events.back().child_id;
  Word(task + 0x260, 4);
  NotifyVsEditorSelection(task + 0x210, 4);
  assert(events.back().choice == 4 && events.back().child_id == picker_id);
  Word(task + 0x278, 3);
  Invoke(BrawlersVsEditorPickerClosed, task + 0x20C);
  assert(events.back().kind == Kind::PickerClosed);

  Word(task + 0x14, 8);
  bytes[task + 0x3A] = 4;
  bytes[task + 0x9A] = 0;
  bytes[task + 0x56] = 2;
  bytes[task + 0x57] = 0;
  bytes[task + 0x58] = 1;
  Word(task + 0x3B8 + 2 * 0x18, 0xE000);
  Word(task + 0x3BC + 2 * 0x18, 0xE00E);
  for (uint32_t i = 0; i < 7; ++i) Unit(0xE000 + i * 2, i + 2);
  bytes[flags] = bytes[flags + 7] = 1;
  bytes[flags + 8] = 0;
  Invoke(BrawlersVsEditorReady, task);
  Caption(u"Synthetic inventory creature");
  assert(events.back().kind == Kind::Resumed && events.back().selection->inventory_count == 7);
  assert(events.back().selection->last_page == 1);
  assert(events.back().selection->displayed_number == 987);
  assert(events.back().selection->level == 10);
  const auto inventory = events.size();
  bytes[task + 0x58] = 2;
  Call(sub_8230F290);
  Caption(u"Invalid final page");
  assert(events.size() == inventory);
  bytes[task + 0x58] = 1;
  Unit(0xE000, 4);
  Caption(u"Invalid vector identity");
  assert(events.size() == inventory);
  Unit(0xE000, 2);
  bytes[task + 0x57] = 1;
  Select(2);
  Caption(u"Past final page length");
  assert(events.size() == inventory);
  bytes[task + 0x57] = 0;
  Select(7);
  Caption(u"Transient arrow");
  assert(events.size() == inventory);
  Select(1);
  bytes[task + 0x3A] = 3;
  Word(task + 0x14, 5);
  bytes[task + 0x5B] = 1;
  Unit(task + 0x62, 17);
  Call(sub_8230F290);
  Caption(u"Synthetic card");
  assert(events.back().selection->item_type == 1 && events.back().selection->item_id == 17);

  Word(task + 0x14, 9);
  Call(sub_82312E18);
  Word(task + 0x14, 11);
  bytes[provider + 0xC] = 0;
  const auto delayed_confirmation = events.size();
  Invoke(BrawlersVsEditorConfirmationReady, task);
  assert(events.size() == delayed_confirmation);
  bytes[provider + 0xC] = 1;
  Word(status, 0x82018DBC);
  Binding(status, task + 0xFC);
  Invoke(BrawlersVsEditorConfirmationPrompt, status, context);
  assert(events[events.size() - 2].kind == Kind::ConfirmationOpened);
  assert(events[events.size() - 2].text == "Synthetic confirmation prompt");
  assert(events.back().kind == Kind::ConfirmationFocused && events.back().choice == 0);
  Word(task + 0x1EC, 1);
  Call(sub_823B9220, task + 0x1E8);
  assert(events.back().kind == Kind::ConfirmationFocused && events.back().choice == 1);
  Word(task + 0x14, 12);
  Word(status, 0x82018DE0);
  Binding(status, task + 0xFC);
  Invoke(BrawlersVsEditorConfirmationStatus, status, context, 0x915);
  assert(events.back().kind == Kind::ConfirmationStatus);
  assert(events.back().text == "Synthetic success status");
  Call(sub_8230F220);
  assert(events.back().kind == Kind::ConfirmationClosed);
  Word(task + 0x14, 5);
  Invoke(BrawlersVsEditorReady, task);
  Caption(u"Synthetic card");
  assert(events.back().kind == Kind::Resumed);
  const auto before_bad = events.size();
  Word(task + 0xD4, 0);
  Call(sub_8230F290);
  Caption(u"Wrong binding");
  assert(events.size() == before_bad);
  Word(task + 0xD4, root + 0x44);
  Unit(widget + 0x104, 0xDC00);
  Invoke(BrawlersVsEditorCaption, widget, task + 0xC4);
  assert(events.size() == before_bad);
  Caption(u"Synthetic card restored");
  assert(events.back().selection->caption == "Synthetic card restored");
  const auto guard_start = events.size();
  Call(sub_8230F290);
  Word(task + 0x18C, 0);
  Caption(u"Invalid graph");
  assert(events.size() == guard_start);
  Word(task + 0x18C, 0x82018A98);
  bytes[task + 0x1A0] = 0;
  Caption(u"Inactive selector");
  assert(events.size() == guard_start);
  bytes[task + 0x1A0] = 1;
  bytes[flags + 1] = 0;
  Caption(u"Disabled selection");
  assert(events.size() == guard_start);
  bytes[flags + 1] = 1;
  Word(0x829CC54C, 0);
  Caption(u"Stale native root");
  assert(events.size() == guard_start);
  Word(0x829CC54C, root);
  Word(loader, 0);
  Caption(u"Wrong loader type");
  assert(events.size() == guard_start);
  Word(loader, 0x82003F2C);
  Word(task + 0x14, 3);
  bytes[task + 0x3A] = 0;
  bytes[task + 0x44] = 1;
  Word(status, 0x82018CF4);
  Binding(status, task + 0xFC);
  Invoke(BrawlersVsEditorStatus, status, context, 0x996);
  assert(events.size() == guard_start);
  Binding(status, task + 0xC4);
  Invoke(BrawlersVsEditorStatus, status, context, 0x996);
  assert(events.back().kind == Kind::Status);
  assert(events.back().text == "Synthetic locked deck status");
  const auto status_count = events.size();
  Invoke(BrawlersVsEditorStatus, status, context, 0x996);
  assert(events.size() == status_count);
  Word(task + 0x14, 8);
  bytes[task + 0x3A] = 4;
  bytes[task + 0x58] = 0;
  Word(task + 0x3BC + 2 * 0x18, 0xE000);
  Unit(task + 0x62, 0xFFFF);
  Call(sub_8230F290);
  Caption(u"");
  assert(events.size() == status_count);
  Invoke(BrawlersVsEditorStatus, status, context, 0x51A);
  assert(events.back().text == "Synthetic empty inventory status");
  bytes[task + 0x3A] = 5;
  bytes[task + 0x9A] = 1;
  bytes[task + 0x56] = 0;
  bytes[task + 0x44] = 0;
  Word(task + 0x3B8 + 6 * 0x18, 0xE000);
  Word(task + 0x3BC + 6 * 0x18, 0xE000);
  Invoke(BrawlersVsEditorReady, task);
  assert(events.back().kind == Kind::EmptyInventory);
  assert(events.back().section_caption == "Synthetic deck");
  const auto empty_count = events.size();
  Caption(u"");
  assert(events.size() == empty_count);
  Call(sub_8231CDA8);
  assert(events.back().kind == Kind::Closed);
  const auto closed = events.size();
  NotifyVsEditorTaskRemoved(task);
  assert(events.size() == closed);
  Fixture();
  Call(sub_82311528);
  Word(summary + 0x14, 6);
  throw_output = true;
  Invoke(BrawlersVsEditorReady, task);
  assert(failures == 1);
  const auto failed_reads = reads;
  Invoke(BrawlersVsEditorReady, task);
  NotifyVsEditorSelection(task + 0x148, 0);
  assert(reads == failed_reads && failures == 1 && !guards.empty());
}
