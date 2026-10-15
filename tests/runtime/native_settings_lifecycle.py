#!/usr/bin/env python3
"""Run the actual settings integration with synthetic native lifecycle boundaries.

Usage: native_settings_lifecycle.py SDK SIMDE_INCLUDE NEW_OUTPUT_DIRECTORY
Requires Linux and bubblewrap. No game, renderer, audio or services are loaded.
The renderer outcome is injected: this verifies orchestration, not geometry.
"""
from pathlib import Path
import os
import shutil
import subprocess
import sys


def inside():
    assert os.environ.get("SETTINGS_LIFECYCLE_ISOLATED") == "1"
    assert not Path("/run/user").exists() and not Path("/dev/dri").exists()
    output = Path("/output")
    stubs = output / "stubs/rex/system"
    stubs.mkdir(parents=True)
    (stubs / "xmemory.h").write_text(r'''
#pragma once
#include <cassert>
#include <cstring>
#include <vector>
namespace rex::memory {
class Memory {
 public:
  std::vector<uint8_t> low = std::vector<uint8_t>(0x100000);
  std::vector<uint8_t> globals = std::vector<uint8_t>(0x30000);
  uint32_t next = 0x80000;
  std::vector<uint32_t> freed;
  uint8_t* Pointer(uint32_t address, size_t size) {
    if (address && uint64_t(address) + size <= low.size()) return low.data() + address;
    if (address >= 0x829C0000 && uint64_t(address) + size <= 0x829F0000)
      return globals.data() + address - 0x829C0000;
    return nullptr;
  }
  bool TryReadVirtual(uint32_t address, void* target, size_t size) {
    auto* source = Pointer(address, size); if (!source) return false;
    std::memcpy(target, source, size); return true;
  }
  template<class T> T TranslateVirtual(uint32_t address) {
    auto* pointer = Pointer(address, 1); assert(pointer); return reinterpret_cast<T>(pointer);
  }
  uint32_t SystemHeapAlloc(uint32_t size) {
    uint32_t result = next; next += (size + 15) & ~15u; assert(next < low.size()); return result;
  }
  void SystemHeapFree(uint32_t address) { freed.push_back(address); }
};
}
''')
    harness = output / "lifecycle.cpp"
    harness.write_text(r'''
#ifndef SETTINGS_SOURCE
#define SETTINGS_SOURCE "/project/src/game/native_settings.cpp"
#endif
#include SETTINGS_SOURCE
#include <cassert>
#include <iostream>
#include <deque>
namespace fixture {
constexpr uint32_t task = 0x1000, model = task + 0x20, primary = 0x2000;
constexpr uint32_t secondary = 0x2100, selector = 0x2200, common = 0x3000, controller = 0x4000;
constexpr uint32_t service = 0x5000, descriptor = 0x6000, image = 0x10000, save_job = 0x60000;
constexpr uint32_t input = 0x30000, cursors = 0x35000, input_vector = 0x36000, pad = 0x37000;
rex::memory::Memory ram;
Catalog catalog;
std::vector<ScreenEvent> messages;
std::vector<std::string> diagnostics;
std::vector<std::string> calls;
std::vector<std::array<uint8_t,6>> snapshots;
bool build_ok = true, publish_ok = true, healthy = true, save_pending = false;
unsigned final_saves = 0, native_requests = 0, native_pumps = 0, final_waits = 0;
unsigned builders = 0, cleanup = 0, removed = 0, render_end = 0;
unsigned selection_enables = 0;
bool render_suspended = false;
View published;
void Word(uint32_t a, uint32_t v) {
  auto* p = ram.Pointer(a, 4); assert(p);
  for (int i = 3; i >= 0; --i) { p[i] = v & 255; v >>= 8; }
}
void ByteAt(uint32_t a, uint8_t v) { *ram.Pointer(a, 1) = v; }
void Half(uint32_t a, uint16_t v) { ByteAt(a, v >> 8); ByteAt(a + 1, v); }
uint32_t Get(uint32_t a) { return *memory.U32BE(a); }
uint8_t GetByte(uint32_t a) { return *memory.U8(a); }
bool Focused(uint32_t slot) {
  // The native list query 8226FB80 combines equality and the enabled byte.
  const auto list=Get(selector+0x28);
  return Get(list+0x10)==slot && GetByte(list+0x18)!=0;
}
void AssertFocus(uint32_t slot) {
  for(uint32_t i=0;i<5;++i) assert(Focused(i)==(i==slot));
}
std::array<uint8_t,6> Values(uint32_t a) {
  std::array<uint8_t,6> result{}; assert(ram.TryReadVirtual(a,result.data(),result.size())); return result;
}
void IdleService() {
  Word(service,0); Word(service+12,0); Word(service+0x24,0);
  Word(0x829CA614,0); Word(0x829CA618,0);
}
void Text() {
  constexpr uint32_t provider=0x40000, bank=0x41000, resource=0x42000;
  Word(0x829CEB78,provider); Word(provider,0x82036508); Word(provider+8,bank); ByteAt(provider+12,1);
  ByteAt(bank+0x60,1); Word(bank,resource); Word(bank+4,resource+0x3020); Word(bank+8,resource+0x20);
  Word(resource,0x70617374); Word(resource+4,0x7274626C); ByteAt(resource+11,1);
  Word(resource+12,0xA30); Word(resource+16,0x20); Word(resource+20,0x3020);
  for (uint32_t id : {0x612u,0x928u,0xA0Du,0x7BEu,0x7BFu,0x7C0u}) {
    const auto offset=0x5000+id*4;
    Word(resource+0x20+id*4,offset); Word(resource+0x20+(id+1)*4,offset+4);
    Half(resource+offset,'S'); Half(resource+offset+2,0);
  }
}
void Audio() {
  constexpr uint32_t audio=0x70000, child=0x71000, pool=0x72000;
  Word(0x829CDCE8,audio); Word(audio,0x82036260); Word(audio+0x40,child); Word(audio+0x3C,pool);
  Word(audio+0x2AC,0x73000); Word(audio+0x2B0,0);
  Word(child,0x82036390); Word(child+4,audio+0x58); Word(child+8,pool);
  Word(child+12,audio+0x29C); Word(child+16,0x73000); Word(child+20,0);
  Word(pool+4,1); Word(pool+0x14,0x74000); Word(pool+0x18,0x74000); Word(pool+0x24,0);
}
void Setup() {
  assert(!session); ram=rex::memory::Memory{};
  messages.clear(); diagnostics.clear(); calls.clear(); snapshots.clear();
  skipped_final_saves.clear(); reported.clear(); pending_notices.clear();
  build_ok=publish_ok=healthy=true; save_pending=false;
  final_saves=native_requests=native_pumps=final_waits=builders=cleanup=removed=render_end=0;
  selection_enables=0;render_suspended=false;
  Word(task,0x82019C40); Word(task+0x14,1); Word(model,2);
  Word(model+8,controller); Word(model+12,selector); Word(model+0x14,primary); Word(model+0x18,secondary);
  ByteAt(model+0x1C,0); Word(selector,0x8200FC30);
  Word(selector+0x28,selector+0x40);Word(selector+0x40,0x8200FC10);
  Word(selector+0x50,0);Word(selector+0x54,7);ByteAt(selector+0x58,0);
  for (auto record : {primary,secondary}) {
    Word(record,0x82019D1C); Word(record+12,selector); Word(record+0x14,model+0x1C);
    Word(record+0x18,model+0x24); Word(record+0x1C,task+0x54);
  }
  const std::array<uint8_t,6> initial{0,1,1,10,10,10};
  std::memcpy(ram.Pointer(model+0x24,6),initial.data(),6);
  Word(common,0x82003EAC); Word(0x829CBC68,common); Half(common+0x20,0x8CC);
  Word(0x829CC350,service); Word(0x829CA610,descriptor); IdleService();
  Word(descriptor,image); Word(descriptor+4,image+16); ByteAt(descriptor+0x108,1); ByteAt(0x829CA40C,0);
  Word(0x829CDA70,input); Word(input,0x82035B48); Word(input+0x1107*4,0);
  Word(0x829CED20,cursors); ByteAt(cursors+0x15,1); Word(0x829E7960,input_vector);
  Word(input_vector+12,input_vector+32); Word(input_vector+16,input_vector+36); Word(input_vector+32,pad);
  ByteAt(pad+12,1); Text(); Audio();
  catalog={};
  for (const char* key : {"hints","restore_defaults","restore_question","yes","no","off","on",
      "display","audio","controls","accessibility","advanced","fullscreen","window_size",
      "window_size_description","vsync","vsync_description","render_scale","render_scale_description",
      "accessibility_audio","friendly_enabled","friendly_volume","enemy_enabled","enemy_volume",
      "save_failed","apply_failed","save_unavailable"}) catalog.text[key]=std::string("Synthetic ")+key;
  catalog.host_values={{Fullscreen,0},{WindowSize,1},{VSync,1},{RenderScale,1},{FriendlyEnabled,1},
      {FriendlyVolume,100},{EnemyEnabled,1},{EnemyVolume,100}};
  catalog.host_defaults=catalog.host_values;
  catalog.window_sizes={{1,"Synthetic window"}}; catalog.render_scales={{1,"Synthetic scale"}};
  for(int i=1;i<=10;++i) catalog.native_volumes.push_back({i,std::to_string(i)});
  for(int i=0;i<=100;i+=10) catalog.cue_volumes.push_back({i,std::to_string(i)});
}
// Contract boundaries independently checked at 82333CD0 (task dispatch),
// 82335E40 (primary animation state), 82334888/82334920 (close readiness), and
// 82334E50 (final save then layer/title cleanup). Start after native readiness;
// asset loading and actual constructors are intentionally outside this fixture.
// Close states are bytes: common+54 stores/loads at 823348D8/8233494C,
// common+9C at 82334908/82334940, controller+8 at 82334914/82334924.
void Animate() {
  const auto phase=GetByte(primary+8);
  if(phase==1 && Get(primary+4)!=1) ByteAt(primary+8,2);
  if(phase==6 && Get(primary+4)!=1) ByteAt(primary+8,7);
  if(phase==3 && Get(primary+4)!=1) {
    ByteAt(primary+8,4); ByteAt(common+0x54,4); ByteAt(common+0x9C,4); ByteAt(controller+8,4);
  }
}
void Tick() {
  PPCContext ctx{}; ctx.r3.u64=task;
  sub_82333CD0(ctx,ram.low.data());
}
void InputTick(uint32_t pressed,uint32_t repeated=0) {
  Word(pad+0x14,pressed|repeated); Word(pad+0x18,pressed); Word(pad+0x20,repeated);
  Tick(); Word(pad+0x14,0); Word(pad+0x18,0); Word(pad+0x20,0);
}
unsigned Opened() {
  return std::count_if(messages.begin(),messages.end(),[](const auto& e){return e.source=="settings" && e.kind=="opened";});
}
}
bool QueueGameDiagnostic(std::string_view value) noexcept {fixture::diagnostics.emplace_back(value);return true;}
bool QueueGameEvent(ScreenEvent&& value) noexcept {fixture::messages.push_back(std::move(value));return true;}
namespace brawlers::settings {
struct HostSettingsRuntime::State {};
HostSettingsRuntime::HostSettingsRuntime()=default;
HostSettingsRuntime::~HostSettingsRuntime()=default;
std::optional<HostCatalogSnapshot> HostSettingsRuntime::ReadCatalogSnapshot() const {return HostCatalogSnapshot{fixture::catalog,1};}
uint64_t HostSettingsRuntime::CatalogRevision() const {return 1;}
std::optional<HostEvent> HostSettingsRuntime::Poll() {return {};}
bool HostSettingsRuntime::HasPendingWork() const {return false;}
bool HostSettingsRuntime::Submit(const Command&) {assert(false);return false;}
void SetNativeSettingsRenderAccess(ui::ReadBytes,std::function<bool(uint32_t,const void*,size_t)>) {}
bool NativeSettingsRenderBegin(uint32_t model,PPCContext&,uint8_t*) {
  assert(model==fixture::model && fixture::Get(model)==2); fixture::calls.emplace_back("begin");return true;
}
bool NativeSettingsRenderBuilt(uint32_t model,PPCContext&,uint8_t*) {
  assert(model==fixture::model && fixture::builders==1); fixture::calls.emplace_back("built");
  fixture::healthy=fixture::build_ok;return fixture::build_ok;
}
bool NativeSettingsRenderPublish(const View& view) {
  fixture::published=view;fixture::calls.emplace_back("publish");
  if(!fixture::publish_ok) fixture::healthy=false;return fixture::publish_ok;
}
uint32_t NativeSettingsRenderSelection() {
  if(fixture::published.confirming) return fixture::published.confirm_yes?1:2;
  return fixture::published.position?fixture::published.position-1:0;
}
bool NativeSettingsRenderHealthy() {return fixture::healthy;}
void NativeSettingsRenderSuspend(bool value) {fixture::render_suspended=value;}
void NativeSettingsRenderEnd(uint32_t model,PPCContext&,uint8_t*) {
  assert(model==fixture::model);++fixture::render_end;fixture::calls.emplace_back("render end");
}
}
extern "C" void sub_82335320(PPCContext&,uint8_t*);
extern "C" void sub_823351E8(PPCContext&,uint8_t*);
extern "C" void sub_82334E50(PPCContext&,uint8_t*);
extern "C" void sub_822E6288(PPCContext&,uint8_t*);
extern "C" void sub_822E6560(PPCContext&,uint8_t*);
extern "C" void __imp__sub_82335320(PPCContext& ctx,uint8_t*) {
  assert(ctx.r3.u32==fixture::model);++fixture::builders;fixture::calls.emplace_back("builder");
  fixture::ByteAt(fixture::primary+8,1);
}
extern "C" void __imp__sub_82333CD0(PPCContext& ctx,uint8_t* base) {
  using namespace fixture;
  assert(ctx.r3.u32==task);const auto phase=Get(task+0x14);PPCContext call=ctx;call.r3.u64=model;
  if(phase==1) {sub_82335320(call,base);Word(task+0x14,2);}
  else if(phase==2) {sub_823351E8(call,base);if(call.r3.u32) Word(task+0x14,3);}
  else if(phase==3) {
    if(GetByte(primary+8)==4 && GetByte(common+0x54)==4 && GetByte(common+0x9C)==4 && GetByte(controller+8)==4)
      Word(task+0x14,4);
  } else if(phase==4) {sub_82334E50(call,base);Word(task+0x14,5);}
  else if(phase==5) {sub_822E6560(call,base);if(call.r3.u32) {++removed;Word(task+0x14,6);}}
}
extern "C" void __imp__sub_823351E8(PPCContext&,uint8_t*) {assert(false);}
extern "C" void __imp__sub_82334888(PPCContext& ctx,uint8_t*) {
  using namespace fixture;assert(ctx.r3.u32==model);ctx.r3.u64=0;
  if(GetByte(primary+8)!=7) return;
  ByteAt(primary+8,3);ByteAt(common+0x54,3);ByteAt(common+0x9C,3);ByteAt(controller+8,3);ctx.r3.u64=1;
}
extern "C" void __imp__sub_82334E50(PPCContext& ctx,uint8_t* base) {
  using namespace fixture;assert(ctx.r3.u32==model && GetByte(model+0x30)!=0xFF);
  ++cleanup;calls.emplace_back("cleanup");
  PPCContext save=ctx;save.r3.u64=0x5500;save.r4.u64=model+0x24;save.r5.u64=0x5E;
  sub_822E6288(save,base);calls.emplace_back("native layers and title removed");
}
extern "C" void __imp__sub_822E6288(PPCContext& ctx,uint8_t*) {
  assert(ctx.r4.u32==fixture::model+0x24);++fixture::final_saves;
  fixture::snapshots.push_back(fixture::Values(ctx.r4.u32));
}
extern "C" void __imp__sub_822E6560(PPCContext& ctx,uint8_t*) {++fixture::final_waits;ctx.r3.u64=1;}
extern "C" void __imp__sub_82333E38(PPCContext&,uint8_t*) {}
extern "C" void __imp__sub_821D7628(PPCContext& ctx,uint8_t*) {
  using namespace fixture;assert(ctx.r3.u32==selector && ctx.r4.u32<7);
  // 821D7628 and 8226FB18 change index/history without activating the list.
  Word(selector+0x34,ctx.r4.u32);Word(selector+0x38,ctx.r4.u32);
  Word(Get(selector+0x28)+0x10,ctx.r4.u32);
}
extern "C" void __imp__sub_821D7670(PPCContext& ctx,uint8_t*) {
  using namespace fixture;assert(ctx.r3.u32==selector && ctx.r4.u32==1);
  const auto list=Get(selector+0x28);assert(list==selector+0x40 && Get(list)==0x8200FC10);
  assert(GetByte(list+0x18)==0);ByteAt(list+0x18,1);++selection_enables;
}
extern "C" void sub_821DCD78(PPCContext& ctx,uint8_t*) {assert(ctx.r3.u32==0 || ctx.r3.u32==1 || ctx.r3.u32==5);}
extern "C" void __imp__sub_823341A0(PPCContext& ctx,uint8_t*) {
  using namespace fixture;assert(ctx.r3.u32==primary && ctx.r5.u32==0 && ctx.r4.u32==0);
  ByteAt(model+0x25,0);
}
extern "C" void __imp__sub_821C0880(PPCContext& ctx,uint8_t*) {
  using namespace fixture;assert(ctx.r3.u32==service && Get(service+12)==0 && ctx.r5.u32==0);
  ++native_requests;Word(service,16);Word(service+12,6);Word(service+0x24,ctx.r4.u32);ctx.r3.u64=1;
}
extern "C" void __imp__sub_822E5D38(PPCContext& ctx,uint8_t*) {
  using namespace fixture;const auto values=Values(Get(ctx.r3.u32+4));snapshots.push_back(values);
  std::memcpy(ram.Pointer(image+16+0x369C,6),values.data(),6);ctx.r3.u64=1;
}
extern "C" void __imp__sub_821C1DA8(PPCContext& ctx,uint8_t* base) {
  using namespace fixture;assert(ctx.r3.u32==service);++native_pumps;
  if(Get(service)==16) {
    PPCContext writer=ctx;writer.r3.u64=Get(service+0x24);writer.r4.u64=image+16;
    __imp__sub_822E5D38(writer,base);Word(service,17);Word(0x829CA618,save_job);Word(save_job+0xDA28,1);
    ctx.r3.u64=1;
  } else if(save_pending) ctx.r3.u64=1;
  else {IdleService();ByteAt(0x829CA61E,1);ctx.r3.u64=2;}
}
extern "C" void __imp__sub_821BC008(PPCContext&,uint8_t*) {assert(false);}
extern "C" void __imp__sub_821CBF18(PPCContext&,uint8_t*) {assert(false);}
extern "C" void __imp__sub_823ADFA8(PPCContext&,uint8_t*) {assert(false);}
extern "C" void __imp__sub_821EB330(PPCContext&,uint8_t*) {assert(false);}
extern "C" void __imp__sub_82212018(PPCContext&,uint8_t*) {assert(false);}
namespace fixture {
void CloseToCleanup() {
  Tick(); assert(Get(task+0x14)==2 && GetByte(primary+8)==6);
  Tick(); assert(Get(task+0x14)==2); // A callback, not another input tick, completes the animation.
  Animate();Tick();assert(Get(task+0x14)==3 && GetByte(primary+8)==3);
  Tick();assert(Get(task+0x14)==3);
  Animate();Tick();assert(Get(task+0x14)==4);
  Tick();assert(Get(task+0x14)==5 && !session && cleanup==1 && render_end==1);
  assert(ram.freed.size()==1);
  const auto native_cleanup=std::find(calls.begin(),calls.end(),"native layers and title removed");
  const auto release=std::find(calls.begin(),calls.end(),"render end");
  assert(native_cleanup!=calls.end() && release!=calls.end() && native_cleanup<release);
}
void FailedOpening(bool fail_publish, bool fail_first_draw = false) {
  Setup();HostSettingsRuntime backend;SetNativeSettings(&ram,&backend);
  build_ok=fail_first_draw || fail_publish;publish_ok=fail_first_draw || !fail_publish;
  Tick();
  if(fail_first_draw) {
    assert(session && !session->failed && !session->opened);
    healthy=false;Tick(); // Native drawing may fail during the opening animation.
  }
  assert(session && session->failed && session->closing && Get(task+0x14)==2);
  assert(builders==1 && Opened()==0 && native_requests==0);
  assert(calls[0]=="begin" && calls[1]=="builder" && calls[2]=="built");
  Tick();assert(GetByte(primary+8)==1 && Get(task+0x14)==2);
  Animate();CloseToCleanup();
  assert(final_saves==0 && snapshots.empty());
  // This task's skipped final request cannot finish while any job is retained.
  for(auto retained : {0x829CA618u,0x829CA614u,service+0x24}) {
    Word(retained,save_job);Tick();assert(Get(task+0x14)==5 && final_waits==0 && removed==0);
    Word(retained,0);
  }
  Tick();assert(Get(task+0x14)==6 && final_waits==0 && removed==1);
}
void SuccessfulOpeningAndEdit(bool later_render_failure) {
  Setup();HostSettingsRuntime backend;SetNativeSettings(&ram,&backend);
  Tick();assert(session && !session->closing && Opened()==0 && !Focused(0) && selection_enables==0);
  Tick();assert(Opened()==0);Animate();Tick();assert(Opened()==1 && Get(task+0x14)==2);
  AssertFocus(0);assert(selection_enables==1);
  Tick();Tick();assert(Opened()==1 && !session->closing && native_requests==0 && final_saves==0);
  // Returning from a native child quarantines its dismissing input, including
  // while the controller is disconnected. Reconnection must release buttons.
  Word(0x829CBC48,1);Tick();assert(session->suspended && render_suspended);
  AssertFocus(0);assert(selection_enables==1); // Suspension hides rows; it does not rewrite native focus.
  Word(0x829CBC48,0);Tick();assert(!session->suspended && session->quarantine);
  AssertFocus(0);assert(!render_suspended && selection_enables==1);
  ByteAt(pad+12,0);Tick();assert(session->quarantine);
  ByteAt(pad+12,1);InputTick(4);assert(session->quarantine && menu.Read().page==1);
  Tick();assert(!session->quarantine && menu.Read().page==1 && Opened()==1);
  // Real input decoding and menu transactions reach the native effect/save seam.
  InputTick(0,0x22000);AssertFocus(1);InputTick(0,0x22000);AssertFocus(2);
  InputTick(4);AssertFocus(0);InputTick(4);AssertFocus(0);
  assert(GetByte(model+0x25)==0 && menu.Read().rows[0].numeric_value==0);
  save_pending=true;
  InputTick(8);InputTick(8); // Return to root and request close while save is pending.
  assert(session->closing && native_requests==1);
  AssertFocus(2);assert(selection_enables==1); // Native exit animation retains its selection.
  Tick();assert(GetByte(primary+8)==2 && Get(task+0x14)==2 && cleanup==0);
  assert(snapshots.size()==1 && snapshots[0][1]==0);
  if(later_render_failure) healthy=false;
  save_pending=false;CloseToCleanup();
  assert(final_saves==1 && snapshots.size()==2 && snapshots[1][1]==0);
  Tick();assert(Get(task+0x14)==6 && final_waits==1 && removed==1);
  assert(Opened()==1);
}
void ConfirmationFocus() {
  Setup();HostSettingsRuntime backend;SetNativeSettings(&ram,&backend);
  Tick();Animate();Tick();AssertFocus(0);
  InputTick(4);assert(menu.Read().page!=1);AssertFocus(0);
  const auto rows=menu.Read().rows.size();assert(rows>1 && rows<=5);
  for(uint32_t i=1;i<rows;++i) {InputTick(0,0x22000);AssertFocus(i);}
  InputTick(4);assert(menu.Read().confirming && !menu.Read().confirm_yes);AssertFocus(2);
  InputTick(0,0x44000);assert(menu.Read().confirm_yes);AssertFocus(1);
  InputTick(0,0x88000);assert(!menu.Read().confirm_yes);AssertFocus(2);
  InputTick(4);assert(!menu.Read().confirming);AssertFocus(rows-1);
  InputTick(8);AssertFocus(0);InputTick(8);assert(session->closing);AssertFocus(0);
  CloseToCleanup();Tick();assert(removed==1 && selection_enables==1);
}
void WrongSelectorOwner() {
  Setup();HostSettingsRuntime backend;SetNativeSettings(&ram,&backend);
  constexpr uint32_t decoy=0x2500;
  Word(decoy,0x8200FC10);Word(decoy+0x14,7);Word(selector+0x28,decoy);
  Tick();assert(session && session->failed && session->closing && selection_enables==0);
  assert(std::find(diagnostics.begin(),diagnostics.end(),"settings_selector_list_owner")!=diagnostics.end());
  assert(Get(decoy+0x10)==0 && GetByte(decoy+0x18)==0);
  Animate();CloseToCleanup();Tick();assert(removed==1 && final_saves==0);
}
}
int main() {
  fixture::FailedOpening(false);fixture::FailedOpening(true);fixture::FailedOpening(false, true);
  fixture::SuccessfulOpeningAndEdit(false);fixture::SuccessfulOpeningAndEdit(true);
  fixture::ConfirmationFocus();fixture::WrongSelectorOwner();
  std::cout<<"PASS: actual integration initialization, selection activation, confirmation focus, input, pending autosave, animation-gated cleanup and final-save policy.\n";
}
''')
    command = ["clang++", "-std=c++23", "-O1", "-g", "-fsanitize=address,undefined",
               "-fno-omit-frame-pointer", "-I/output/stubs", "-I/project/src/core", "-I/project/src/game",
               "-isystem", "/sdk/include", "-isystem", "/simde", str(harness),
               "/project/src/core/settings_menu.cpp", "/project/src/core/settings_catalog.cpp",
               "/project/src/core/settings_events.cpp", "/project/src/game/native_settings_save.cpp"]
    subprocess.run(command + ["-o", "/output/lifecycle"], check=True)
    subprocess.run(["/output/lifecycle"], check=True)
    # Restore the old decision only in a private copy: this regression must fail.
    source = Path("/project/src/game/native_settings.cpp").read_text()
    old = "skip_final_save = untouched_failure ||"
    if old not in source:
        raise AssertionError("Update negative control after cleanup policy changes")
    mutant = output / "old-cleanup-policy.cpp"
    mutant.write_text(source.replace(old, "skip_final_save =", 1))
    subprocess.run(command + ['-DSETTINGS_SOURCE="/output/old-cleanup-policy.cpp"',
                             "-o", "/output/old-policy"], check=True)
    result = subprocess.run(["/output/old-policy"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (output / "negative-control.log").write_bytes(result.stdout)
    assert result.returncode != 0 and b"final_saves==0" in result.stdout, result.stdout.decode()
    print("PASS: restoring the previous final-save policy fails the initialization-failure regression.")
    activation = '''    Call(__imp__sub_821D7670, session->selector, 1);
    if (!Guard(memory.U8(*child + 0x18) == 1, "settings_selector_enabled_result")) return false;'''
    if activation not in source:
        raise AssertionError("Update negative control after selection activation changes")
    unfocused = output / "index-without-activation.cpp"
    unfocused.write_text(source.replace(activation, "    // Negative control: index alone leaves native focus disabled.", 1))
    subprocess.run(command + ['-DSETTINGS_SOURCE="/output/index-without-activation.cpp"',
                             "-o", "/output/unfocused-policy"], check=True)
    result = subprocess.run(["/output/unfocused-policy"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (output / "focus-negative-control.log").write_bytes(result.stdout)
    assert result.returncode != 0 and b"Focused(i)==(i==slot)" in result.stdout, result.stdout.decode()
    print("PASS: correct selected index without native activation fails the visible-focus regression.")


def main():
    if sys.argv[1:] == ["--inside"]:
        inside()
        return
    if sys.platform != "linux" or len(sys.argv) != 4:
        raise SystemExit(__doc__)
    bwrap = shutil.which("bwrap")
    if not bwrap:
        raise SystemExit("bubblewrap required; no unisolated fallback")
    sdk, simde, output = [Path(value).resolve() for value in sys.argv[1:]]
    project = Path(__file__).resolve().parents[2]
    if output == project or project in output.parents:
        raise SystemExit("Output must be outside the source tree")
    output.mkdir(exist_ok=False)
    command = [bwrap,"--unshare-all","--die-with-parent","--new-session"]
    for directory in ("/usr","/bin","/lib","/lib64"):
        if Path(directory).exists(): command += ["--ro-bind",directory,directory]
    if Path("/etc/ld.so.cache").is_file(): command += ["--ro-bind","/etc/ld.so.cache","/etc/ld.so.cache"]
    command += ["--proc","/proc","--dev","/dev","--tmpfs","/tmp","--tmpfs","/run",
                "--ro-bind",str(project),"/project","--ro-bind",str(sdk),"/sdk",
                "--ro-bind",str(simde),"/simde","--bind",str(output),"/output","--clearenv"]
    for key,value in {"PATH":"/usr/bin:/bin","HOME":"/tmp/home","XDG_RUNTIME_DIR":"/run/private",
        "XDG_CONFIG_HOME":"/tmp/config","XDG_DATA_HOME":"/tmp/data","XDG_CACHE_HOME":"/tmp/cache",
        "DBUS_SESSION_BUS_ADDRESS":"unix:path=/run/private/session","DBUS_SYSTEM_BUS_ADDRESS":"unix:path=/run/private/system",
        "AT_SPI_BUS_ADDRESS":"unix:path=/run/private/accessibility","NO_AT_BRIDGE":"1",
        "PYTHONDONTWRITEBYTECODE":"1","PYTHONNOUSERSITE":"1","SETTINGS_LIFECYCLE_ISOLATED":"1"}.items():
        command += ["--setenv",key,value]
    command += ["--chdir","/output","python3","/project/tests/runtime/native_settings_lifecycle.py","--inside"]
    subprocess.run(command,check=True)


if __name__ == "__main__":
    main()
