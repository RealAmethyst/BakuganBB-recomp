// Synthetic viewer parents and draw events; no game or live services.
#include "native_vs_viewer.h"

#include <cassert>
#include <cfenv>
#include <map>
#include <stdexcept>
#include <vector>

#include <rex/ppc/context.h>
#include <rex/thread/mutex.h>

extern "C" void sub_8230E9F8(PPCContext&, uint8_t*);
extern "C" void sub_822DC058(PPCContext&, uint8_t*);
extern "C" void sub_822DD1E0(PPCContext&, uint8_t*);
void BrawlersVsViewerReady(PPCRegister&);
void BrawlersVsViewerSuspended(PPCRegister&);
void BrawlersVsViewerName(PPCRegister&, PPCRegister&);
void BrawlersVsViewerDescription(PPCRegister&, PPCRegister&);
void NotifyVsEditorViewerOpened(uint32_t) noexcept {}

std::recursive_mutex& rex::thread::global_critical_region::mutex() {
  static std::recursive_mutex lock;
  return lock;
}
namespace {
constexpr uint32_t task = 0x1000, root = 0x3000, summary = 0x4000;
constexpr uint32_t editor = 0x5000, manager = 0x6000;
constexpr uint32_t provider = 0x8000, bank = 0x9000, resource = 0x10000;
std::map<uint32_t, uint8_t> bytes;
std::vector<ScreenEvent> events;
std::vector<std::string> diagnostics;
unsigned reads = 0, originals = 0, failures = 0;
bool throw_output = false;
uint8_t opaque[1]{};
void Word(uint32_t address, uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) bytes[address + i] = value >> (24 - i * 8);
}
void Unit(uint32_t address, uint16_t value) {
  bytes[address] = value >> 8;
  bytes[address + 1] = value;
}
void Text(uint32_t id, const char16_t* value) {
  uint32_t address = resource + 0x10000 + id * 0x100;
  do { Unit(address, *value); address += 2; } while (*value++);
}
void Fixture() {
  Word(0x829CC54C, root);
  Word(root, 0x8201A078);
  Word(root + 0x14, 8);
  Word(summary, 0x82017DDC);
  Word(summary + 0x14, 6);
  Word(summary + 0x18, root + 0x42C);
  Word(summary + 0x1F4, root + 0x44);
  Word(editor, 0x82018DE8);
  Word(editor + 0x14, 5);
  Word(editor + 0x18, 0);
  Word(editor + 0x1C, root + 0x44 + 0xF8);
  Word(editor + 0x2C, summary + 0x200);
  bytes[editor + 0x3A] = 2;
  bytes[editor + 0x38] = 2;
  Word(0x829CC330, manager);
  Word(manager, 0x82003F2C);
  Unit(manager + 0x20, 0x9D4);
  Word(task, 0x82017294);
  Word(task + 0x14, 0);
  Word(task + 0x18, manager + 0x2C);
  Word(task + 0x20, 0x82017278);
  bytes[task + 0xB0] = 2;
  Word(0x829CEB78, provider);
  Word(provider, 0x82036508);
  Word(provider + 8, bank);
  bytes[provider + 0xC] = 1;
  bytes[bank + 0x60] = 1;
  Word(bank, resource);
  Word(bank + 4, resource + 0x5000);
  Word(bank + 8, resource + 0x20);
  Word(resource, 0x70617374);
  Word(resource + 4, 0x7274626C);
  bytes[resource + 0xB] = 1;
  Word(resource + 0xC, 0x1000);
  Word(resource + 0x10, 0x20);
  Word(resource + 0x14, 0x5000);
  for (uint32_t i = 0; i < 0x1000; ++i) Word(resource + 0x20 + i * 4, 0x10000 + i * 0x100);
  Text(0xDB4, u"Synthetic creature");
  Text(0x345, u"Synthetic biography");
  Text(0xA08, u"Synthetic Back hint");
}
bool Read(uint32_t address, void* target, size_t size) {
  ++reads;
  auto* destination = static_cast<uint8_t*>(target);
  for (size_t i = 0; i < size; ++i) {
    const auto found = bytes.find(address + i);
    if (found == bytes.end()) return false;
    destination[i] = found->second;
  }
  return true;
}
PPCRegister Register(uint32_t value) { PPCRegister result{}; result.u64 = value; return result; }
void Construct() {
  PPCContext context{};
  context.r3.u64 = task;
  context.r4.u64 = manager + 0x2C;
  context.r7.u64 = 2;
  sub_822DC058(context, opaque);
  assert(context.r12.u64 == 55);
}
void Preview() {
  PPCContext context{};
  context.r3.u64 = editor;
  context.r14.u64 = 0x123456789ABCDEF0;
  sub_8230E9F8(context, opaque);
  assert(context.r3.u64 == 19 && context.r14.u64 == 0x123456789ABCDEF0);
  Word(task + 0x14, 3);
  auto owner = Register(task);
  BrawlersVsViewerReady(owner);
}
void Name(uint32_t model = task + 0x20) {
  auto context = Register(model), id = Register(0xDB4);
  BrawlersVsViewerName(context, id);
}
void Description(uint32_t id = 0x345) {
  auto context = Register(task + 0x20), text = Register(id);
  BrawlersVsViewerDescription(context, text);
}
}

extern "C" void __imp__sub_8230E9F8(PPCContext& context, uint8_t* base) {
  assert(base == opaque && context.r3.u64 == editor);
  ++originals;
  Construct();
  Word(editor + 0x14, 0x10);
  context.r3.u64 = 19;
}
extern "C" void __imp__sub_822DC058(PPCContext& context, uint8_t* base) {
  assert(base == opaque);
  ++originals;
  context.r12.u64 = 55;
}
extern "C" void __imp__sub_822DD1E0(PPCContext& context, uint8_t* base) {
  assert(base == opaque);
  ++originals;
  context.r3.u64 = 77;
}

int main() {
  SetVsViewerCapture(Read, [](ScreenEvent&& event) {
    if (throw_output) throw std::runtime_error("synthetic sink failure");
    events.push_back(std::move(event));
  }, [](std::string_view reason) { diagnostics.emplace_back(reason); }, [] { ++failures; });
  Fixture();
  Construct();  // A viewer from another native entry is not a VS child.
  auto owner = Register(task);
  BrawlersVsViewerReady(owner);
  Name(); Description();
  assert(events.empty());
  Preview();
  Name(task + 0x24);  // A readable neighbouring model is not the registered owner.
  Description();
  assert(events.empty());
  Name();
  assert(events.size() == 1 && events[0].kind == "opened");
  assert(events[0].body == "Synthetic creature\nSynthetic biography");
  assert(events[0].controller == 2);
  assert(events[0].title.empty() && events[0].label.empty() && events[0].total == 0);
  assert(events[0].hints.size() == 1 && events[0].hints[0] == "Synthetic Back hint");
  assert(diagnostics.empty());
  const auto identity = events[0].id;
  const auto idle_reads = reads;
  for (int i = 0; i < 50; ++i) { Name(); Description(); }
  assert(events.size() == 1 && reads == idle_reads);
  BrawlersVsViewerSuspended(owner);
  NotifyVsViewerTaskRemoved(task);
  NotifyVsViewerTaskRemoved(task);
  assert(events.size() == 2 && events.back().kind == "closed" && events.back().id == identity);

  // Parent ownership rejects a plausible but unrelated completion pointer.
  Fixture();
  Word(summary + 0x18, root + 0x428);
  Preview(); Name(); Description();
  assert(events.size() == 2 && diagnostics.back() == "vs_viewer_summary");
  Fixture();
  Word(editor + 0x1C, root + 0x44 + 4);
  Preview(); Name(); Description();
  assert(events.size() == 2 && diagnostics.back() == "vs_viewer_participant");
  Fixture();
  Unit(manager + 0x20, 0x9D3);
  Preview(); Name(); Description();
  assert(events.size() == 2 && diagnostics.back() == "vs_viewer_resource");

  // Do not publish a partial biography or start a second native lifetime while
  // the actual localized text is temporarily unreadable.
  Fixture();
  Preview();
  bytes.erase(resource + 0x10000 + 0x345 * 0x100);
  Name(); Description();
  assert(events.size() == 2);
  Text(0x345, u"Synthetic later biography");
  Description();
  assert(events.size() == 3 && events.back().id != identity);
  assert(events.back().body == "Synthetic creature\nSynthetic later biography");
  PPCContext destructor{};
  destructor.r3.u64 = task;
  sub_822DD1E0(destructor, opaque);
  assert(destructor.r3.u64 == 77 && events.back().kind == "closed");

  // A readable empty optional biography is different from an inaccessible one.
  Fixture();
  Text(0x345, u"");
  Preview(); Name(); Description();
  assert(events.back().kind == "opened" && events.back().body == "Synthetic creature");
  assert(events.back().hints.size() == 1);
  NotifyVsViewerTaskRemoved(task);

  Fixture();
  Preview();
  Name();
  BrawlersVsViewerSuspended(owner);
  const auto count = events.size();
  Description();
  assert(events.size() == count);
  NotifyVsViewerTaskRemoved(task);
  Fixture();
  Preview(); Name();
  throw_output = true;
  const auto rounding = std::fegetround();
  Description();
  assert(failures == 1 && std::fegetround() == rounding);
  const auto failed_reads = reads;
  Name(); Description();
  assert(failures == 1 && reads == failed_reads);
  assert(originals == 18);
}
