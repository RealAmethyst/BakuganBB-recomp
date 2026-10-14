#!/usr/bin/env python3
"""Original synthetic fixture for the patched runtime's actual message dialog.

Run only in isolation without live desktop, accessibility or audio resources.
Usage: message-box-regressions.py PATCHED_SDK NEW_OUTPUT_DIRECTORY
"""
from pathlib import Path
import subprocess
import sys

runtime = Path(sys.argv[1]).resolve()
root = Path(sys.argv[2]).resolve()
root.mkdir(exist_ok=False)
source = (runtime / 'src/kernel/xam/xam_ui.cpp').read_text()
observer=source[source.index('namespace rex::ui {'):source.index('namespace rex {\nnamespace kernel {')]
start=source.index('class MessageBoxDialog :')
dialog=source[start:source.index('// https://www.se7ensins',start)]
start=source.index('auto close = [result_ptr](MessageBoxDialog*')
completion=source[start:source.index('    const Runtime* emulator',start)]
harness=r'''
#include <atomic>
#include <cassert>
#include <cstdint>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>
#include <imgui.h>
#include <rex/ui/keyboard_dialog.h>
#include <rex/ui/message_box_dialog.h>
#include <rex/ui/modal_gamepad_input.h>
static int observer_failures=0;
#define REXKRNL_ERROR(...) ++observer_failures
namespace rex::ui { class ImGuiDrawer { public: void SetModalGamepadTarget(uint64_t) {} }; }
using X_RESULT=uint32_t;
constexpr X_RESULT X_ERROR_SUCCESS=0;
class XamDialog {
 public:
  explicit XamDialog(rex::ui::ImGuiDrawer*) {}
  virtual ~XamDialog() = default;
  virtual void OnDraw(ImGuiIO&) = 0;
  void Draw() { if(!closed) { OnDraw(ImGui::GetIO()); if(pending) { OnClose(); closed=true; } } }
  void set_close_callback(std::function<void()> callback) { callback_=std::move(callback); }
  bool closed=false;
 protected:
  rex::ui::ImGuiDrawer* imgui_drawer() { return nullptr; }
  void Close() { pending=true; }
  virtual void OnClose() { if(callback_) callback_(); }
 private:
  bool pending=false;
  std::function<void()> callback_;
};
'''
harness+=observer+'\nnamespace rex::kernel::xam {\n'+dialog+'\n}\n'
harness+='using MessageBoxDialog=rex::kernel::xam::MessageBoxDialog;\nauto Completion(uint32_t* result_ptr) {\n'+completion+'\nreturn close; }\n'
harness+=r'''
using Event=rex::ui::MessageBoxDialogEvent;
using Dialog=MessageBoxDialog;
static std::vector<Event> events;
static void Frame(Dialog& dialog) { ImGui::NewFrame(); dialog.Draw(); ImGui::EndFrame(); }
static void Frames(Dialog& dialog,int count=3) { while(count--) Frame(dialog); }
static void Key(Dialog& dialog,ImGuiKey key) {
 ImGui::GetIO().AddKeyEvent(key,true); Frame(dialog);
 ImGui::GetIO().AddKeyEvent(key,false); Frames(dialog);
}
static size_t Count(Event::Kind kind) {
 size_t count=0; for(const auto& e:events) if(e.kind==kind) ++count; return count;
}
static const Event& Last(Event::Kind kind) {
 for(auto it=events.rbegin();it!=events.rend();++it) if(it->kind==kind) return *it;
 assert(false); return events.front();
}
int main() {
 ImGui::CreateContext();
 auto& io=ImGui::GetIO(); io.DisplaySize=ImVec2(800,600); io.DeltaTime=1.0f/60;
 io.IniFilename=nullptr; io.LogFilename=nullptr;
 unsigned char* pixels;int width,height; io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
 rex::ui::SetMessageBoxDialogObserver([](const Event& e){events.push_back(e);});
 uint64_t first_id=0;
 {
  Dialog dialog(nullptr,"Synthetic##hidden","Literal##body",{"First##hidden","Second","Third"},2);
  uint32_t result=UINT32_MAX, completion_count=0;
  dialog.set_close_callback([&]{
   assert(Count(Event::Kind::Closed)==1);
   assert(Completion(&result)(&dialog)==X_ERROR_SUCCESS); ++completion_count;
  });
  Frame(dialog);
  assert(Count(Event::Kind::Opened)==1 && events[0].title=="Synthetic");
  assert(events[0].description=="Literal##body" && events[0].position==0 && events[0].total==3);
  first_id=events[0].id;
  Frames(dialog);
  assert(Count(Event::Kind::Focused)==1);
  assert(Last(Event::Kind::Focused).label=="Third" && Last(Event::Kind::Focused).position==3);
  // Actual initial focus follows the native requested default.
  assert(dialog.chosen_button()==2);
  size_t idle_count=events.size(); Frames(dialog); assert(events.size()==idle_count);
  assert(!(io.ConfigFlags & ImGuiConfigFlags_NavEnableKeyboard));
  Key(dialog,ImGuiKey_Enter); assert(!dialog.closed);
  // The observer does not add navigation bindings. Enable the existing ImGui
  // navigation option only in the fixture to exercise actual focus movement.
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  Key(dialog,ImGuiKey_Tab); // First reveals the navigation cursor on the current button.
  Key(dialog,ImGuiKey_LeftArrow);
  assert(Last(Event::Kind::Focused).label=="Second" && Last(Event::Kind::Focused).position==2);
  assert(Count(Event::Kind::Opened)==1);
  Key(dialog,ImGuiKey_Enter);
  assert(dialog.closed && result==1 && completion_count==1);
  assert(Count(Event::Kind::Closed)==1 && Last(Event::Kind::Closed).label.empty());
  assert(Last(Event::Kind::Closed).position==0 && Last(Event::Kind::Closed).total==3);
  Frames(dialog); assert(completion_count==1);
  assert(io.ConfigFlags & ImGuiConfigFlags_NavEnableKeyboard);
 }
 {
  events.clear(); Dialog dialog(nullptr,"Synthetic##hidden","",{"Only"},0);
  Frames(dialog); assert(events[0].id!=first_id);
  assert(Last(Event::Kind::Focused).label=="Only" && Last(Event::Kind::Focused).total==1);
  assert(Last(Event::Kind::Focused).position==1);
  Key(dialog,ImGuiKey_Enter); assert(dialog.closed && dialog.chosen_button()==0);
 }
 {
  events.clear(); Dialog dialog(nullptr,"","Empty title fallback",{"Accept"},0);
  Frames(dialog); assert(events[0].title=="Message Box");
  Key(dialog,ImGuiKey_Enter); assert(dialog.closed);
 }
 {
  events.clear(); Dialog dialog(nullptr,"##no-visible-title","Body",{"##hidden-button"},0);
  Frames(dialog); assert(events[0].title.empty());
  assert(Last(Event::Kind::Focused).label.empty() && Last(Event::Kind::Focused).position==1);
  Key(dialog,ImGuiKey_Enter); assert(dialog.closed);
 }
 {
  rex::ui::SetMessageBoxDialogObserver([](const Event&){throw std::runtime_error("synthetic");});
  Dialog dialog(nullptr,"Throwing observer","",{"Continue"},0);
  uint32_t result=UINT32_MAX;
  dialog.set_close_callback([&]{assert(Completion(&result)(&dialog)==0);});
  Frames(dialog); Key(dialog,ImGuiKey_Enter);
  assert(dialog.closed && result==0 && observer_failures>=3);
 }
 {
  rex::ui::SetMessageBoxDialogObserver({});
  Dialog dialog(nullptr,"No observer","",{"Continue"},0);
  Frames(dialog); Key(dialog,ImGuiKey_Enter); assert(dialog.closed && dialog.chosen_button()==0);
 }
 ImGui::DestroyContext();
 std::cout << "PASS: actual ImGui message-box open/focus/close, title/button captions, actual requested default focus, no idle repetition, existing nav unchanged, selection/completion, one choice, throwing/absent observers.\n";
}
'''
p=root/'synthetic-message-box.cpp'
p.write_text(harness)
imgui=runtime/'thirdparty/imgui'
command=['clang++','-std=c++23','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(imgui),'-I'+str(runtime/'include'),str(p)]
command += [str(imgui/f) for f in ['imgui.cpp','imgui_draw.cpp','imgui_tables.cpp','imgui_widgets.cpp']]
command += ['-o',str(root/'synthetic-message-box')]
subprocess.run(command,check=True)
subprocess.run([str(root/'synthetic-message-box')],check=True)
