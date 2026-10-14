"""Exercise the patched runtime's keyboard dialog with original synthetic input.

Run inside an isolated test environment. No game inputs or UI backends are used.
The output directory must not already exist. Requires Clang and pinned SDK sources.
"""
from pathlib import Path
import argparse
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("runtime", type=Path, help="SDK tree with keyboard-dialog.patch applied")
parser.add_argument("output", type=Path, help="new directory for generated fixture and binary")
args = parser.parse_args()
runtime = args.runtime.resolve()
root = args.output.resolve()
root.mkdir(parents=True, exist_ok=False)
source = (runtime/'src/kernel/xam/xam_ui.cpp').read_text()
observer = source[source.index('namespace rex::ui {'):source.index('namespace rex {\nnamespace kernel {')]
completion_start = source.index('auto close = [buffer, buffer_length](KeyboardInputDialog*')
completion = source[completion_start:source.index('    const Runtime* emulator', completion_start)]
guard_start = source.index('  constexpr size_t kMaxKeyboardCapacity')
guard = source[guard_start:source.index('  assert_true(overlapped', guard_start)]
dialog = source[source.index('class KeyboardInputDialog :'):source.index('// https://www.se7ensins', source.index('class KeyboardInputDialog :'))]
harness = r'''
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstring>
#include <iostream>
#include <limits>
#include <utf8.h>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>
#include <imgui.h>
#include <rex/ui/keyboard_dialog.h>
#if __has_include(<rex/ui/message_box_dialog.h>)
#include <rex/ui/message_box_dialog.h>
#endif
#define REXKRNL_ERROR(...) ++observer_failures
static int observer_failures = 0;
namespace rex::ui { class ImGuiDrawer {}; }
namespace rex::string {
std::string to_utf8(std::u16string_view s) { return utf8::utf16to8(s); }
std::u16string to_utf16(std::string_view s) { return utf8::utf8to16(s); }
void copy_and_swap_truncating(char16_t* dest, std::u16string_view source, size_t count) {
 size_t n=std::min(source.size(), count-1);
 for(size_t i=0;i<n;++i) dest[i]=(source[i]>>8)|(source[i]<<8);
 dest[n]=0;
}
void copy_truncating(char* destination, const std::string& text, size_t size) {
  assert(size); size_t count = std::min(text.size(), size-1);
  std::memcpy(destination, text.data(), count); destination[count] = 0;
}
}
class XamDialog {
 public:
  explicit XamDialog(rex::ui::ImGuiDrawer*) {}
  virtual ~XamDialog() = default;
  virtual void OnDraw(ImGuiIO&) = 0;
  void Draw() { if (!closed) { OnDraw(ImGui::GetIO()); if (pending) { OnClose(); closed = true; } } }
  bool closed = false;
 protected:
  void Close() { pending = true; }
  virtual void OnClose() {}
  ImGuiIO& GetIO() { return ImGui::GetIO(); }
 private:
  bool pending = false;
};
'''
harness += observer + '\nnamespace rex::kernel::xam {\n' + dialog + '\n}\n'
harness += 'using X_RESULT = uint32_t; constexpr uint32_t X_ERROR_SUCCESS=0, X_ERROR_CANCELLED=1223, X_ERROR_INVALID_PARAMETER=87;\n'
harness += 'using KeyboardInputDialog = rex::kernel::xam::KeyboardInputDialog;\nauto Completion(char16_t* buffer,size_t buffer_length) {\n' + completion + '\nreturn close; }\n'
harness += 'uint32_t Guard(char16_t* buffer, uint32_t buffer_length) {\n' + guard + '\nreturn 0; }\n'
harness += r'''
using Event = rex::ui::KeyboardDialogEvent;
using Dialog = rex::kernel::xam::KeyboardInputDialog;
static std::vector<Event> events;
static std::string clipboard;
static void Frame(Dialog& dialog) {
  ImGui::NewFrame(); dialog.Draw(); ImGui::EndFrame();
}
static void Frames(Dialog& dialog, int count=3) { while(count--) Frame(dialog); }
static void Key(Dialog& dialog, ImGuiKey key) {
  ImGui::GetIO().AddKeyEvent(key,true); Frame(dialog);
  ImGui::GetIO().AddKeyEvent(key,false); Frames(dialog);
}
static const Event& Last(Event::Kind kind) {
  auto it = std::find_if(events.rbegin(),events.rend(),[&](auto& e){return e.kind==kind;});
  assert(it!=events.rend()); return *it;
}
static size_t Count(Event::Kind kind) {
  return std::count_if(events.begin(),events.end(),[&](auto& e){return e.kind==kind;});
}
int main() {
  ImGui::CreateContext();
  auto& io=ImGui::GetIO(); io.DisplaySize=ImVec2(800,600); io.DeltaTime=1.0f/60;
  io.IniFilename=nullptr; io.LogFilename=nullptr;
  unsigned char* pixels; int width,height; io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
  rex::ui::SetKeyboardDialogObserver([](const Event& e){events.push_back(e);});
  {
    Dialog d(nullptr,"Synthetic##hidden","Synthetic description##literal","seed",32);
    Frames(d);
    assert(Count(Event::Kind::Opened)==1);
    assert(events[0].text=="seed" && events[0].capacity==32);
    assert(events[0].title=="Synthetic" && events[0].description=="Synthetic description##literal");
    assert(Count(Event::Kind::Focused)==1 && Count(Event::Kind::Edited)==0);
    assert(Last(Event::Kind::Focused).label.empty());
    assert(Last(Event::Kind::Focused).cursor>=0);
    size_t idle_events=events.size(); Frames(d); assert(events.size()==idle_events);
    io.AddInputCharactersUTF8("a"); Frames(d);
    assert(Count(Event::Kind::Edited)==1);
    assert(Last(Event::Kind::Edited).text=="a"); // ImGui selects default on initial focus.
    io.AddInputCharactersUTF8("é"); Frames(d);
    assert(Last(Event::Kind::Edited).text=="aé" && Last(Event::Kind::Edited).cursor==3);
    Key(d,ImGuiKey_LeftArrow);
    assert(Last(Event::Kind::Edited).cursor==1);
    io.AddKeyEvent(ImGuiMod_Shift,true); Key(d,ImGuiKey_RightArrow);
    io.AddKeyEvent(ImGuiMod_Shift,false); Frames(d);
    const auto selection=Last(Event::Kind::Edited);
    assert(std::abs(selection.selection_start-selection.selection_end)==2);
    Key(d,ImGuiKey_Backspace); assert(Last(Event::Kind::Edited).text=="a");
    Key(d,ImGuiKey_Tab); assert(Last(Event::Kind::Focused).label=="OK");
    assert(Last(Event::Kind::Focused).position==1 && Last(Event::Kind::Focused).total==2);
    Key(d,ImGuiKey_Tab); assert(Last(Event::Kind::Focused).label=="Cancel");
    assert(Last(Event::Kind::Focused).position==2);
    io.AddKeyEvent(ImGuiMod_Shift,true); Key(d,ImGuiKey_Tab);
    io.AddKeyEvent(ImGuiMod_Shift,false); Frames(d);
    assert(Last(Event::Kind::Focused).label=="OK");
    Key(d,ImGuiKey_Enter);
    assert(d.closed && !d.cancelled() && d.text()=="a");
    assert(Count(Event::Kind::Closed)==1 && !Last(Event::Kind::Closed).cancelled);
    assert(!(io.ConfigFlags & ImGuiConfigFlags_NavEnableKeyboard));
  }
  uint64_t first_id=events.front().id;
  events.clear();
  {
    Dialog d(nullptr,"Synthetic title","","original",32); Frames(d);
    assert(events[0].id!=first_id);
    io.AddInputCharactersUTF8("changed"); Frames(d);
    auto edits=Count(Event::Kind::Edited); Key(d,ImGuiKey_Escape);
    assert(d.closed && d.cancelled() && d.text().empty());
    assert(Last(Event::Kind::Closed).cancelled && Last(Event::Kind::Closed).text=="changed");
    assert(Count(Event::Kind::Edited)==edits && Count(Event::Kind::Closed)==1);
    assert(!(io.ConfigFlags & ImGuiConfigFlags_NavEnableKeyboard));
  }
  events.clear();
  {
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    Dialog d(nullptr,"Enter test","","",32); Frames(d);
    io.AddInputCharactersUTF8("confirmed"); Frames(d); Key(d,ImGuiKey_Enter);
    assert(d.closed && !d.cancelled() && d.text()=="confirmed");
    assert(io.ConfigFlags & ImGuiConfigFlags_NavEnableKeyboard);
    io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
  }
  {
    rex::ui::SetKeyboardDialogObserver([](const Event&){throw std::runtime_error("synthetic");});
    Dialog d(nullptr,"Observer exception","","",32); Frames(d); Key(d,ImGuiKey_Escape);
    assert(d.closed && d.cancelled() && observer_failures>=3);
    assert(!(io.ConfigFlags & ImGuiConfigFlags_NavEnableKeyboard));
  }
  rex::ui::SetKeyboardDialogObserver({});
  {
    Dialog d(nullptr,"No observer","","",32); Frames(d); Key(d,ImGuiKey_Escape);
    assert(d.closed && d.cancelled());
  }

  rex::ui::SetKeyboardDialogObserver([](const Event& e){events.push_back(e);});
  ImGui::GetPlatformIO().Platform_GetClipboardTextFn=[](ImGuiContext*){return clipboard.c_str();};
  auto paste=[](Dialog& d,const char* text) {
    clipboard=text;
    ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl,true); Key(d,ImGuiKey_V);
    ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl,false); Frames(d);
  };
  {
    events.clear(); Dialog d(nullptr,"ASCII capacity","","",5); Frames(d);
    paste(d,"abcd"); assert(Last(Event::Kind::Edited).text=="abcd");
    auto before=events.size(); io.AddInputCharactersUTF8("e"); Frames(d);
    assert(events.size()==before); // Whole excess edit is rejected.
    Key(d,ImGuiKey_Home); Key(d,ImGuiKey_RightArrow);
    auto cursor=Last(Event::Kind::Edited).cursor;
    paste(d,"XYZ"); assert(Last(Event::Kind::Edited).text=="abcd");
    assert(Last(Event::Kind::Edited).cursor==cursor);
    Key(d,ImGuiKey_Enter); assert(d.text()=="abcd");
  }
  {
    events.clear(); Dialog d(nullptr,"Accented capacity","","ab",5); Frames(d);
    Key(d,ImGuiKey_End); Key(d,ImGuiKey_LeftArrow); // Insert between a and b.
    paste(d,"éé"); assert(Last(Event::Kind::Edited).text=="aééb");
    paste(d,"é"); assert(Last(Event::Kind::Edited).text=="aééb");
    Key(d,ImGuiKey_Enter); assert(d.text()=="aééb");
  }
  for (auto value : std::vector<std::pair<std::string,std::string>>{
      {"abc","abc"},{"abcdef","abc"},{"éééé","ééé"},{"a😀","a😀"},{"ab😀","ab"}}) {
    events.clear(); Dialog d(nullptr,"Default boundary","",value.first,4); Frames(d);
    assert(events[0].text==value.second);
    Key(d,ImGuiKey_Enter); assert(d.text()==value.second);
  }
  {
    events.clear(); Dialog d(nullptr,"Minimum capacity","","abc",1); Frames(d);
    assert(events[0].text.empty()); paste(d,"a");
    Key(d,ImGuiKey_Enter); assert(d.text().empty());
  }
  {
    events.clear(); Dialog d(nullptr,"Supplementary preserved","","a😀b",5); Frames(d);
    Key(d,ImGuiKey_End); Key(d,ImGuiKey_LeftArrow);
    paste(d,"é"); assert(Last(Event::Kind::Edited).text=="a😀b");
    Key(d,ImGuiKey_Enter); assert(d.text()=="a😀b");
  }

  {
    char16_t output[5]={0xAAAA,0xAAAA,0xAAAA,0xAAAA,0xAAAA};
    Dialog d(nullptr,"Native cancel callback","","original",5); Frames(d);
    Key(d,ImGuiKey_Escape);
    uint32_t error=0,length=99;
    assert(Completion(output,5)(&d,error,length)==X_ERROR_CANCELLED);
    assert(error==X_ERROR_CANCELLED && length==0);
    for(auto unit:output) assert(unit==0xAAAA);
  }
  {
    char16_t output[5]={};
    Dialog d(nullptr,"Native success callback","","aé😀",5); Frames(d);
    Key(d,ImGuiKey_Enter); uint32_t error=99,length=99;
    assert(Completion(output,5)(&d,error,length)==X_ERROR_SUCCESS);
    assert(error==0 && length==0);
    assert(output[0]==0x6100 && output[1]==0xE900 && output[2]==0x3DD8 && output[3]==0x00DE && output[4]==0);
    assert(Guard(output,0)==X_ERROR_INVALID_PARAMETER && Guard(nullptr,1)==X_ERROR_INVALID_PARAMETER);
    constexpr uint32_t maximum=(std::numeric_limits<int>::max()-1)/3+1;
    assert(Guard(output,1)==0 && Guard(output,maximum)==0);
    assert(Guard(output,maximum+1)==X_ERROR_INVALID_PARAMETER);
    assert(Guard(output,UINT32_MAX)==X_ERROR_INVALID_PARAMETER);
  }
  ImGui::DestroyContext();
  std::cout << "PASS: real ImGui open/focus/edit/UTF8 cursor/selection/delete/Tab/Shift-Tab/Enter/Escape; observer exceptions; config restoration; optional observer; UTF16 capacity/default/surrogate/mid-string paste boundaries; actual completion callbacks/cancel-no-write/guard boundaries.\n";
}
'''
path=root/'synthetic-dialog.cpp'
path.write_text(harness)
imgui=runtime/'thirdparty/imgui'
command=['clang++','-std=c++23','-g','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(imgui),'-I'+str(runtime/'thirdparty/utfcpp/source'),'-I'+str(runtime/'include'),str(path)]
command += [str(imgui/f) for f in ['imgui.cpp','imgui_draw.cpp','imgui_tables.cpp','imgui_widgets.cpp']]
command += ['-o',str(root/'synthetic-dialog')]
subprocess.run(command,check=True)
subprocess.run([str(root/'synthetic-dialog')],check=True)
