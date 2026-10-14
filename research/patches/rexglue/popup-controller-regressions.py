#!/usr/bin/env python3
"""Exercise actual popup/channel code without SDL devices or a UI backend.

Run only in an isolated environment without desktop, input, or service access.
Usage: popup-controller-regressions.py PATCHED_SDK NEW_OUTPUT_DIRECTORY
"""
from pathlib import Path
import subprocess
import sys

sdk = Path(sys.argv[1]).resolve()
out = Path(sys.argv[2]).resolve()
out.mkdir(exist_ok=False)
source = (sdk / 'src/kernel/xam/xam_ui.cpp').read_text()
observer = source[source.index('namespace rex::ui {'):source.index('namespace rex {\nnamespace kernel {')]
start = source.index('class MessageBoxDialog :')
dialog = source[start:source.index('// https://www.se7ensins', start)]
start = source.index('auto close = [result_ptr](MessageBoxDialog*')
completion = source[start:source.index('    const Runtime* emulator', start)]
drawer = (sdk / 'src/ui/imgui_drawer.cpp').read_text()
start = drawer.index('  auto gamepad = window_->modal_gamepad_input();')
frame_input = drawer[start:drawer.index('  ImGui::NewFrame();', start)]
start = drawer.index('  gamepad->SetTarget(next_gamepad_target_);')
frame_end = drawer[start:drawer.index('  ImDrawData* draw_data', start)]
harness = r'''
#include <atomic>
#include <cassert>
#include <functional>
#include <iostream>
#include <thread>
#include <vector>
#include <imgui.h>
#include <rex/ui/keyboard_dialog.h>
#include <rex/ui/message_box_dialog.h>
#include <rex/ui/modal_gamepad_input.h>
#define REXKRNL_ERROR(...) ((void)0)
using Input=rex::ui::ModalGamepadInput;
namespace rex::ui {
struct ImGuiDrawer {
 uint64_t target=0;
 void SetModalGamepadTarget(uint64_t value) { target=value; }
};
}
using X_RESULT=uint32_t;
constexpr X_RESULT X_ERROR_SUCCESS=0;
class XamDialog {
 public:
  explicit XamDialog(rex::ui::ImGuiDrawer* drawer):drawer_(drawer) {}
  virtual ~XamDialog()=default;
  virtual void OnDraw(ImGuiIO&)=0;
  void Draw() { if(!closed) { OnDraw(ImGui::GetIO()); if(pending) { OnClose(); closed=true; } } }
  void set_close_callback(std::function<void()> value) { callback_=std::move(value); }
  bool closed=false;
 protected:
  rex::ui::ImGuiDrawer* imgui_drawer() { return drawer_; }
  void Close() { pending=true; }
  virtual void OnClose() { if(callback_) callback_(); }
 private:
  rex::ui::ImGuiDrawer* drawer_;
  bool pending=false;
  std::function<void()> callback_;
};
'''
harness += observer + '\nnamespace rex::kernel::xam {\n' + dialog + '\n}\n'
harness += 'using MessageBoxDialog=rex::kernel::xam::MessageBoxDialog;\nusing Dialog=MessageBoxDialog;\nauto Completion(uint32_t* result_ptr) {\n' + completion + '\nreturn close; }\n'
harness += r'''
using Event=rex::ui::MessageBoxDialogEvent;
static std::vector<Event> events;
static constexpr uint16_t A=0x1000, Left=4, Right=8;
static uint64_t sequence=1;
static void Publish(const std::shared_ptr<Input>& input,uint16_t button,Input::Button nav,bool down) {
 input->Publish(17,button,nav,down,++sequence);
}
struct Window { std::shared_ptr<Input> input=std::make_shared<Input>();
 auto modal_gamepad_input() { return input; } };
struct Harness {
 Window window;
 Window* window_=&window;
 rex::ui::ImGuiDrawer drawer;
 uint64_t& next_gamepad_target_=drawer.target;
 void Frame(Dialog& dialog) {
  auto& io=ImGui::GetIO();
  using ModalGamepadInput=Input;
'''
harness += frame_input
harness += '  ImGui::NewFrame(); dialog.Draw(); ImGui::Render();\n' + frame_end + '\n }\n'
harness += r'''
 void Frames(Dialog& dialog,int count=4) { while(count--) Frame(dialog); }
 void Press(Dialog& dialog,uint16_t button,Input::Button nav) {
  Publish(window.input,button,nav,true); Publish(window.input,button,nav,false);
  Frames(dialog,6);
 }
};
static const Event& LastFocus() {
 for(auto it=events.rbegin();it!=events.rend();++it)
  if(it->kind==Event::Kind::Focused) return *it;
 assert(false); return events.front();
}
static void ChannelTests() {
 auto input=std::make_shared<Input>();
 input->Connect(17,0,0,sequence++); input->SetFocus(true);
 auto capture=input->RequestCapture(); input->SetTarget(capture->id());
 assert(input->TakeFrame().reset);
 Publish(input,A,Input::Confirm,true); Publish(input,A,Input::Confirm,false);
 auto frame=input->TakeFrame(); assert(frame.edge && frame.edge->down);
 frame=input->TakeFrame(); assert(frame.edge && !frame.edge->down);
 assert(!input->TakeFrame().edge);
 // A raw release cannot unblock the older cached guest press.
 capture->Release();
 assert(input->FilterGuest(17,A,sequence-1).buttons==0);
 assert(input->FilterGuest(17,0,sequence).buttons==0);
 Publish(input,A,Input::Confirm,true);
 assert(input->FilterGuest(17,A,sequence).buttons==A);
 // A fresh release/repress between guest polls remains a legitimate press.
 capture=input->RequestCapture(); input->SetTarget(capture->id());
 capture->Release(); Publish(input,A,Input::Confirm,false);
 Publish(input,A,Input::Confirm,true);
 assert(input->FilterGuest(17,A,sequence).buttons==A);
 // Held opening input and input during loss of focus require release.
 capture=input->RequestCapture(); input->SetTarget(capture->id());
 assert(input->TakeFrame().reset); assert(!input->TakeFrame().edge);
 input->SetFocus(false); Publish(input,Right,Input::Right,true);
 input->SetFocus(true); assert(input->TakeFrame().reset); assert(!input->TakeFrame().edge);
 Publish(input,Right,Input::Right,false); Publish(input,Right,Input::Right,true);
 assert(input->TakeFrame().edge->down);
 // A newly attached non-message child invalidates the old frame target.
 input->SetTarget(0); Publish(input,A,Input::Confirm,false); Publish(input,A,Input::Confirm,true);
 assert(input->TakeFrame().reset); assert(!input->TakeFrame().active);
 input->SetTarget(capture->id()); assert(input->TakeFrame().reset); assert(!input->TakeFrame().edge);
 // A child target clears queued activation; returning cannot replay it.
 auto child=input->RequestCapture(); input->SetTarget(child->id());
 input->TakeFrame(); Publish(input,A,Input::Confirm,false); Publish(input,A,Input::Confirm,true);
 child->Release(); input->SetTarget(capture->id());
 assert(input->TakeFrame().reset); assert(!input->TakeFrame().edge);
 input->Disconnect(17); assert(input->FilterGuest(17,A,sequence).suppress);
 input->Connect(18,A,Input::Confirm,++sequence); input->TakeFrame();
 assert(!input->TakeFrame().edge);
 input->Publish(18,A,Input::Confirm,false,++sequence);
 input->Publish(18,A,Input::Confirm,true,++sequence);
 assert(input->TakeFrame().edge->down);
 // Overflow discards the incomplete stream and releases ImGui state.
 for(int i=0;i<150;++i) input->Publish(18,A,Input::Confirm,(i%2)==0,++sequence);
 assert(input->overflow_count()>0 && input->TakeFrame().reset);
 input->Fail(); assert(!input->TakeFrame().active);
 assert(input->FilterGuest(18,0,sequence).suppress);
 // Independently running raw publication and frame/guest consumers.
 auto concurrent=std::make_shared<Input>(); concurrent->Connect(99,0,0,1);
 concurrent->SetFocus(true); auto lease=concurrent->RequestCapture(); concurrent->SetTarget(lease->id());
 std::thread producer([&]{for(uint64_t i=2;i<20002;++i) concurrent->Publish(99,A,Input::Confirm,i%2,i);});
 for(int i=0;i<20000;++i) { concurrent->TakeFrame(); assert(concurrent->FilterGuest(99,A,0).suppress); }
 producer.join();
}
int main() {
 ChannelTests(); ImGui::CreateContext();
 auto& io=ImGui::GetIO(); io.DisplaySize=ImVec2(800,600);io.DeltaTime=1.0f/60;
 io.IniFilename=nullptr;io.LogFilename=nullptr;
 unsigned char* pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
 rex::ui::SetMessageBoxDialogObserver([](const Event& event){events.push_back(event);});
 {
  Harness h; auto input=h.window.input; input->SetFocus(true); input->Connect(17,0,0,++sequence);
  auto capture=input->RequestCapture();
  Dialog dialog(&h.drawer,"Synthetic##hidden","Body",{"First##hidden","Second","Third"},1,capture);
  uint32_t result=99,closed=0;
  dialog.set_close_callback([&]{assert(Completion(&result)(&dialog)==0);++closed;});
  h.Frames(dialog);
  assert(LastFocus().position==2 && LastFocus().label=="Second");
  assert(events.front().title=="Synthetic");
  assert(!(io.ConfigFlags&ImGuiConfigFlags_NavEnableGamepad));
  assert(!(io.BackendFlags&ImGuiBackendFlags_HasGamepad));
  assert(!(io.ConfigFlags&ImGuiConfigFlags_NavEnableKeyboard));
  h.Press(dialog,Right,Input::Right); assert(LastFocus().position==3);
  h.Press(dialog,Left,Input::Left); assert(LastFocus().position==2);
  // No navigation role is assigned to other native buttons or sticks.
  h.Press(dialog,0x2000,Input::None); assert(LastFocus().position==2 && !dialog.closed);
  h.Press(dialog,A,Input::Confirm);
  assert(dialog.closed && closed==1 && result==1 && !input->IsCaptured());
  assert(input->FilterGuest(17,A,sequence-1).buttons==0);
  size_t opens=0,closes=0;for(const auto& event:events) {
   opens+=event.kind==Event::Kind::Opened; closes+=event.kind==Event::Kind::Closed;
  } assert(opens==1 && closes==1);
 }
 ImGui::DestroyContext();
 std::cout<<"PASS: actual ImGui popup/default focus, Left/Right/A, exact observer captions and completion; channel rapid edges, sequenced guest handoff, focus, nested capture, reconnection, overflow, failure and concurrent consumers.\n";
}
'''
p = out / 'synthetic-popup-controller.cpp'
p.write_text(harness)
imgui = sdk / 'thirdparty/imgui'
command = ['clang++', '-std=c++23', '-O1', '-g', '-pthread', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-I'+str(imgui), '-I'+str(sdk/'include'), str(p)]
command += [str(imgui/f) for f in ['imgui.cpp', 'imgui_draw.cpp', 'imgui_tables.cpp', 'imgui_widgets.cpp']]
command += ['-o', str(out/'synthetic-popup-controller')]
subprocess.run(command,check=True)
subprocess.run([str(out/'synthetic-popup-controller')],check=True)

# Compile the real watch, buffering and drain methods against a tiny driver
# shell. SDL event declarations are real; only device/system calls are stubs.
sdl_source = (sdk/'src/input/sdl/sdl_input_driver.cpp').read_text()
start = sdl_source.index('uint16_t XButtonFromSDL(')
helpers = sdl_source[start:sdl_source.index('}  // namespace',start)]
start = sdl_source.index('bool SDLInputDriver::EventWatch(')
methods = sdl_source[start:sdl_source.index('void SDLInputDriver::ProcessEventLocked(',start)]
input_source = (sdk/'include/rex/input/input.h').read_text()
start = input_source.index('enum X_INPUT_GAMEPAD_BUTTON {')
buttons = input_source[start:input_source.index('};',start)+2]
header = (sdk/'include/rex/input/sdl/sdl_input_driver.h').read_text()
start = header.index('  struct PendingEvent {')
fields = header[start:header.index('  uint64_t processed_event_sequence_',start)]
fields += '  uint64_t processed_event_sequence_=0;\n'
driver = r'''
#include <SDL3/SDL.h>
#include <rex/ui/modal_gamepad_input.h>
#include <array>
#include <atomic>
#include <cassert>
#include <iostream>
#include <mutex>
#include <thread>
#include <type_traits>
#include <vector>
extern "C" int SDL_PeepEvents(SDL_Event*,int,SDL_EventAction,Uint32,Uint32) { return 0; }
extern "C" void SDL_FlushEvents(Uint32,Uint32) {}
'''+buttons+'\n'+helpers+r'''
class SDLInputDriver {
 public:
  static bool EventWatch(void*,SDL_Event*);
  void HandleEvent(const SDL_Event&);
  std::unique_lock<std::mutex> DrainAndLock();
  void QueueControllerUpdate() { ++queued; }
  void ProcessEventLocked(const SDL_Event& event) {
   if(event.type==SDL_EVENT_GAMEPAD_BUTTON_DOWN) buttons|=XButtonFromSDL(event.gbutton.button);
   if(event.type==SDL_EVENT_GAMEPAD_BUTTON_UP) buttons&=~XButtonFromSDL(event.gbutton.button);
   ++processed;
  }
  std::shared_ptr<rex::ui::ModalGamepadInput> modal_input_=std::make_shared<rex::ui::ModalGamepadInput>();
  std::mutex controllers_mutex_,event_queue_mutex_;
  uint32_t sdl_events_unflushed_=0;
  uint16_t buttons=0;
  uint64_t processed=0;
  std::atomic<uint64_t> queued=0;
'''+fields+'};\n'+methods+r'''
static SDL_Event Button(bool down,uint8_t button=SDL_GAMEPAD_BUTTON_SOUTH) {
 SDL_Event event{};event.type=down?SDL_EVENT_GAMEPAD_BUTTON_DOWN:SDL_EVENT_GAMEPAD_BUTTON_UP;
 event.gbutton.which=42;event.gbutton.button=button;event.gbutton.down=down;return event;
}
int main() {
 using Input=rex::ui::ModalGamepadInput;
 assert(NavigationButtonFromSDL(SDL_GAMEPAD_BUTTON_DPAD_LEFT)==Input::Left);
 assert(NavigationButtonFromSDL(SDL_GAMEPAD_BUTTON_DPAD_RIGHT)==Input::Right);
 assert(NavigationButtonFromSDL(SDL_GAMEPAD_BUTTON_SOUTH)==Input::Confirm);
 for(int button=0;button<SDL_GAMEPAD_BUTTON_COUNT;++button)
  if(button!=SDL_GAMEPAD_BUTTON_DPAD_LEFT && button!=SDL_GAMEPAD_BUTTON_DPAD_RIGHT && button!=SDL_GAMEPAD_BUTTON_SOUTH)
   assert(NavigationButtonFromSDL(button)==Input::None);
 SDLInputDriver driver; auto input=driver.modal_input_;
 input->Connect(42,0,0,0);input->SetFocus(true);
 auto capture=input->RequestCapture();input->SetTarget(capture->id());input->TakeFrame();
 auto down=Button(true),up=Button(false);
 assert(!SDLInputDriver::EventWatch(&driver,&down));
 { auto lock=driver.DrainAndLock(); assert(driver.buttons==X_INPUT_GAMEPAD_A); }
 capture->Release();
 SDLInputDriver::EventWatch(&driver,&up);
 assert(input->FilterGuest(42,driver.buttons,driver.processed_event_sequence_).buttons==0);
 { auto lock=driver.DrainAndLock(); assert(driver.buttons==0); }
 SDLInputDriver::EventWatch(&driver,&down);
 { auto lock=driver.DrainAndLock();assert(input->FilterGuest(42,driver.buttons,driver.processed_event_sequence_).buttons==X_INPUT_GAMEPAD_A); }
 // Two drains and one producer must preserve the buffered event order.
 std::atomic<bool> done=false;
 std::thread producer([&]{for(int i=0;i<20000;++i) { auto event=Button(i%2);SDLInputDriver::EventWatch(&driver,&event); }done=true;});
 std::thread consumer([&]{while(!done) { auto lock=driver.DrainAndLock(); }});
 while(!done) { auto lock=driver.DrainAndLock(); }
 producer.join();consumer.join();
 { auto lock=driver.DrainAndLock();assert(driver.processed==20003);assert(driver.buttons==X_INPUT_GAMEPAD_A); }
 assert(driver.queued>0);
 std::cout<<"PASS: actual SDL watch mapping, ordered buffering, sequenced release handoff, two concurrent drains.\n";
}
'''
p=out/'synthetic-popup-driver.cpp'
p.write_text(driver)
command=['clang++','-std=c++23','-O1','-g','-pthread','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(sdk/'thirdparty/sdl3/include'),'-I'+str(sdk/'include'),str(p),'-o',str(out/'synthetic-popup-driver')]
subprocess.run(command,check=True)
subprocess.run([str(out/'synthetic-popup-driver')],check=True)

# Exercise the actual dispatch template, including UI-thread construction,
# completion installation, failed dispatch and asynchronous result propagation.
start=source.index('template <typename T>\nX_RESULT xeXamDispatchDialog(')
dispatch=source[start:source.index('template <typename T>\nX_RESULT xeXamDispatchDialogEx',start)]
factory=r'''
#include <rex/ui/modal_gamepad_input.h>
#include <cassert>
#include <chrono>
#include <functional>
#include <iostream>
#include <memory>
#include <vector>
using X_RESULT=uint32_t;
constexpr X_RESULT X_ERROR_CANCELLED=1223,X_ERROR_IO_PENDING=997;
static bool on_ui=false;
static std::function<void()> pending_close;
static unsigned xam_dialogs_shown_=0;
#define REXKRNL_ERROR(...) ((void)0)
namespace rex::thread {
struct Fence { void Wait() { assert(xam_dialogs_shown_==1);on_ui=true;pending_close();on_ui=false;pending_close={}; } };
inline void Sleep(std::chrono::milliseconds) {}
}
namespace rex::ui {
struct WindowedAppContext {
 bool available=true;
 bool CallInUIThreadSynchronous(std::function<void()> fn) {
  if(!available) return false;
  assert(!on_ui);on_ui=true;fn();on_ui=false;return true;
 }
};
}
struct Runtime {
 rex::ui::WindowedAppContext context;
 rex::ui::WindowedAppContext* app_context() { return &context; }
};
struct Kernel {
 Runtime runtime;
 X_RESULT async_result=99;
 std::vector<bool> notifications;
 Runtime* emulator() { return &runtime; }
 void BroadcastNotification(int id,bool active) { assert(id==9);notifications.push_back(active); }
 void CompleteOverlappedDeferred(std::function<X_RESULT()> run,uint32_t overlapped,std::function<void()> pre,std::function<void()> post) {
  assert(overlapped==17);pre();async_result=run();post();
 }
} kernel;
#define REX_KERNEL_STATE() (&::kernel)
namespace rex::kernel::xam {
'''+dispatch+r'''
}
struct Dialog {
 std::function<void()> close;
 Dialog() { assert(on_ui); }
 void set_close_callback(std::function<void()> value) { assert(on_ui);close=std::move(value); }
 void Then(rex::thread::Fence*) { assert(on_ui && close);pending_close=[this]{close();delete this;}; }
};
int main() {
 auto input=std::make_shared<rex::ui::ModalGamepadInput>();
 auto capture=input->RequestCapture();unsigned created=0,completed=0;
 auto create=[&] { ++created;return new Dialog; };
 auto close=[&](Dialog*) -> X_RESULT {assert(on_ui);++completed;return 42;};
 auto result=rex::kernel::xam::xeXamDispatchDialog<Dialog>(create,capture,close,0);
 assert(result==42 && created==1 && completed==1 && !input->IsCaptured());
 assert(kernel.notifications==std::vector<bool>({true,false}) && !xam_dialogs_shown_);
 kernel.runtime.context.available=false;capture=input->RequestCapture();
 result=rex::kernel::xam::xeXamDispatchDialog<Dialog>(create,capture,close,0);
 assert(result==X_ERROR_CANCELLED && created==1 && completed==1 && !input->IsCaptured());
 kernel.runtime.context.available=true;capture=input->RequestCapture();
 result=rex::kernel::xam::xeXamDispatchDialog<Dialog>([]() -> Dialog* {throw 1;},capture,close,0);
 assert(result==X_ERROR_CANCELLED && completed==1 && !input->IsCaptured());
 capture=input->RequestCapture();result=rex::kernel::xam::xeXamDispatchDialog<Dialog>(create,capture,close,17);
 assert(result==X_ERROR_IO_PENDING && kernel.async_result==42 && completed==2 && !input->IsCaptured());
 std::cout<<"PASS: actual dispatch creates and installs completion on UI thread; synchronous/asynchronous results and failure release.\n";
}
'''
p=out/'synthetic-popup-dispatch.cpp';p.write_text(factory)
command=['clang++','-std=c++23','-O1','-g','-pthread','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(sdk/'include'),str(p),'-o',str(out/'synthetic-popup-dispatch')]
subprocess.run(command,check=True)
subprocess.run([str(out/'synthetic-popup-dispatch')],check=True)
