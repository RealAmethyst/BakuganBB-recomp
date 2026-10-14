// Semantic observations for the pinned, statically translated executable.
// Original consumer evidence is recorded in docs/localization-consumers.md.
#include "native_game_ui.h"
#include "native_speech.h"
#include "game_speech_queue.h"

#include <cfenv>
#include <cstdio>
#include <array>
#include <map>
#include <optional>
#include <stdexcept>
#include <set>
#include <string>

#include <rex/ppc/context.h>
#include <rex/thread/mutex.h>

#define ORIGINAL(address) extern "C" void __imp__sub_##address(PPCContext&, uint8_t*)
ORIGINAL(822CED20);
ORIGINAL(822CF058);
ORIGINAL(822DA190);
ORIGINAL(821E9C30);
ORIGINAL(821EA650);
ORIGINAL(822DADA0);
ORIGINAL(823B60B8);
ORIGINAL(823B87A8);
ORIGINAL(822CFDF8);
ORIGINAL(822DAD58);
ORIGINAL(823AFCB8);
ORIGINAL(823AFF38);
ORIGINAL(822D38D0);
ORIGINAL(822D30D8);
ORIGINAL(822D3A50);
ORIGINAL(822D3830);
ORIGINAL(822D6688);
ORIGINAL(822D6238);
ORIGINAL(822D5F48);
ORIGINAL(822D6AD8);
ORIGINAL(822D6798);
ORIGINAL(822D6368);
ORIGINAL(822D6050);
ORIGINAL(822D6BD0);
ORIGINAL(821DFBE8);
ORIGINAL(821DF478);
ORIGINAL(821E1790);
ORIGINAL(823A9528);
ORIGINAL(823A9908);
ORIGINAL(823A9A38);
ORIGINAL(821DF130);
#undef ORIGINAL

namespace {

using brawlers::ui::GuestUiMemory;
GuestUiMemory memory({});
rex::thread::global_critical_region state_region;

brawlers::ui::GameSpeechQueue& Output() {
  static brawlers::ui::GameSpeechQueue queue(SendScreenEvent, [](std::string_view reason) {
    return std::fprintf(stderr, "BRAWLERS_GAME_UI:guard=%.*s\n",
                        static_cast<int>(reason.size()), reason.data()) >= 0;
  });
  return queue;
}
uint64_t next_identity = 0;
std::set<std::string> reported_guards;

struct Screen {
  uint64_t id = 0;
  uint64_t decoder_generation_floor = 0;
  bool opened = false;
  bool active = false;
  bool resume = false;
  bool verified = false;
  uint32_t locale = 0;
  uint32_t selection = 0;
  uint32_t description_id = 0;
  uint32_t text_root = 0;
  std::map<uint32_t, std::string> text;
  std::string last_item;
  std::string hints;
  bool hints_sent = false;
  uint32_t total = 0;
  bool focused = false;
  uint32_t bank = 0;
  std::string body;
};

std::map<uint32_t, Screen> menus;
std::map<uint32_t, Screen> titles;
std::map<uint32_t, Screen> logos;
std::map<uint32_t, Screen> saves;
std::map<uint32_t, Screen> popups;
thread_local uint32_t drawing_movie = 0;

struct MoviePicture {
  uint32_t codec = 0;
  uint64_t generation = 0;
  uint32_t object = 0;
};
struct MovieDecoder {
  uint32_t state = 0;
  uint64_t generation = 0;
  uint64_t next_object = 0;
  std::optional<uint32_t> current, started;
  std::map<uint32_t, uint32_t> pictures;
};
struct MovieOutput {
  uint32_t player = 0, frame = 0, byte_count = 0;
  std::optional<MoviePicture> selected;
};
uint64_t next_movie_generation = 0;
std::map<uint32_t, MovieDecoder> movie_decoders;
std::map<uint32_t, uint32_t> movie_states;
std::map<std::pair<uint32_t, uint32_t>, MoviePicture> movie_frames;
thread_local MovieOutput movie_output;

enum class TextScope {
  None, MenuDescription, MenuHints, TitlePrompt,
  SaveName, SaveDate, SaveTime, SaveEmpty, SaveHints, PopupBody, PopupHints
};
thread_local TextScope text_scope = TextScope::None;
thread_local uint32_t text_owner = 0;
thread_local uint32_t building_save = 0;
struct SaveWidget { uint32_t owner; TextScope scope; };
std::map<uint32_t, SaveWidget> save_widgets;

class FloatEnvironment {
 public:
  FloatEnvironment() : valid_(std::fegetenv(&value_) == 0) {}
  ~FloatEnvironment() { if (valid_) std::fesetenv(&value_); }
 private:
  std::fenv_t value_;
  bool valid_;
};

bool Guard(bool valid, const char* reason) {
  if (!valid && reported_guards.emplace(reason).second)
    Output().EnqueueDiagnostic(reason);
  return valid;
}

template <typename Function>
void Observe(Function function) noexcept {
  FloatEnvironment preserve;
  try {
    auto lock = state_region.Acquire();
    try {
      function();
    } catch (...) {
      // A missing lifetime event makes later output unsafe. No IO under this lock.
      Output().FailCapture();
    }
  } catch (...) {
    // A failed runtime lock cannot be bypassed with an unprotected guest read.
  }
}

bool Object(uint32_t owner, uint32_t table, const char* reason) {
  return Guard(owner && owner <= UINT32_MAX - 0x100 && memory.U32BE(owner) == table, reason);
}

ScreenEvent Event(const char* source, const char* kind, const Screen& screen) {
  ScreenEvent event;
  event.source = source;
  event.kind = kind;
  event.id = screen.id;
  event.locale = screen.locale;
  return event;
}

void CommonResource(Screen& screen) {
  const auto root = memory.U32BE(0x829CBC68);
  if (!Guard(root.has_value(), "common_resource_global") ||
      !Object(*root, 0x82003EAC, "common_resource_owner"))
    return;
  const auto resource = memory.U16BE(*root + 0x20);
  if (!Guard(resource.has_value() && *resource >= 0x8CC && *resource <= 0x8D2,
             "common_resource_locale"))
    return;
  screen.text_root = *root;
  screen.locale = *resource - 0x8CB;
}

void EmitPopup(Screen& screen) {
  if (!screen.active || screen.body.empty())
    return;
  if (!screen.opened) {
    auto event = Event("popup", "opened", screen);
    event.body = screen.body;
    Output().Enqueue(std::move(event));
    screen.opened = true;
  }
  if (!screen.focused || !screen.verified || !screen.locale || !screen.total)
    return;
  const auto fingerprint = std::to_string(screen.selection);
  if (fingerprint == screen.last_item && (screen.hints.empty() || screen.hints_sent))
    return;
  auto event = Event("popup", "focused", screen);
  event.label_key = screen.total == 1 ? "popup_ok" :
                    (screen.selection == 0 ? "popup_yes" : "popup_no");
  event.position = screen.selection + 1;
  event.total = screen.total;
  if (!screen.hints.empty()) event.hints.push_back(screen.hints);
  Output().Enqueue(std::move(event));
  screen.last_item = fingerprint;
  screen.hints_sent = !screen.hints.empty();
}

void EmitSave(Screen& screen) {
  if (!screen.active || !screen.opened || !screen.focused || !screen.verified)
    return;
  std::string label, description;
  const auto empty = screen.text.find(0xA3F);
  if (empty != screen.text.end()) {
    label = empty->second;
  } else {
    const auto name = screen.text.find(0);
    const auto date = screen.text.find(0xE75);
    const auto time = screen.text.find(0xE76);
    if (name == screen.text.end() || date == screen.text.end() || time == screen.text.end())
      return;
    label = name->second;
    if (!label.empty()) label += ", ";
    label += date->second;
    description = time->second;
  }
  if (label.empty()) return;
  const auto fingerprint = label + "\n" + description;
  if (fingerprint == screen.last_item && !screen.resume &&
      (screen.hints.empty() || screen.hints_sent))
    return;
  auto event = Event("save", screen.resume ? "resumed" : "focused", screen);
  event.label = label;
  event.description = description;
  event.position = screen.selection + 1;
  event.total = screen.total;
  if (!screen.hints.empty()) event.hints.push_back(screen.hints);
  Output().Enqueue(std::move(event));
  screen.last_item = fingerprint;
  screen.resume = false;
  screen.hints_sent = !screen.hints.empty();
}

void EmitMenu(Screen& screen) {
  if (!screen.verified || !screen.active || !screen.opened || !screen.locale)
    return;
  const auto description = screen.text.find(screen.description_id);
  if (description == screen.text.end())
    return;
  const auto fingerprint = std::to_string(screen.selection) + ":" + description->second;
  if (fingerprint == screen.last_item && !screen.resume &&
      (screen.hints.empty() || screen.hints_sent))
    return;
  auto event = Event("menu", screen.resume ? "resumed" : "focused", screen);
  event.label_key = "main_" + std::to_string(screen.selection);
  event.description = description->second;
  event.position = screen.selection + 1;
  event.total = 5;
  if (!screen.hints.empty())
    event.hints.push_back(screen.hints);
  Output().Enqueue(std::move(event));
  screen.last_item = fingerprint;
  screen.resume = false;
  screen.hints_sent = !screen.hints.empty();
}

void EmitTitle(Screen& screen) {
  if (!screen.verified || !screen.active || !screen.opened)
    return;
  const auto prompt = screen.text.find(0x9E5);
  if (prompt == screen.text.end() || (prompt->second == screen.last_item && !screen.resume))
    return;
  auto event = Event("title", screen.resume ? "resumed" : "focused", screen);
  event.label = prompt->second;
  Output().Enqueue(std::move(event));
  screen.last_item = prompt->second;
  screen.resume = false;
}

void MainState(uint32_t owner, uint32_t state) {
  const auto found = menus.find(owner);
  if (found == menus.end())
    return;
  auto& screen = found->second;
  if (!Guard(state == 4 || state == 5, "menu_transition_value")) {
    screen.active = false;
    return;
  }
  screen.active = state == 4;
  if (screen.active && !screen.opened) {
    Output().Enqueue(Event("menu", "opened", screen));
    screen.opened = true;
  }
  EmitMenu(screen);
}

void MainSelection(uint32_t owner, uint32_t selected, uint32_t description) {
  const auto found = menus.find(owner);
  if (found == menus.end())
    return;
  auto& screen = found->second;
  screen.verified = false;
  if (!Guard(selected < 5, "menu_selection_event") ||
      !Guard(description == 0x613 + selected || description == 0xA43,
             "menu_description_event"))
    return;
  screen.selection = selected;
  screen.description_id = description;
  screen.verified = true;
  EmitMenu(screen);
}

void TitleState(uint32_t owner, uint32_t state) {
  const auto found = titles.find(owner);
  if (found == titles.end())
    return;
  auto& screen = found->second;
  screen.verified = false;
  if (!Guard(state == 3 || state == 4 || state == 6 || state == 9 || state == 13,
             "title_transition_value")) {
    screen.active = false;
    return;
  }
  const bool was_active = screen.active;
  screen.active = state == 3;
  if (!screen.active)
    return;
  screen.verified = true;
  if (!screen.opened) {
    auto event = Event("title", "opened", screen);
    event.title_key = "title";
    Output().Enqueue(std::move(event));
    screen.opened = true;
  } else if (!was_active) {
    screen.resume = true;
  }
  EmitTitle(screen);
}

void NewScreen(std::map<uint32_t, Screen>& screens, uint32_t owner, uint32_t table,
               const char* reason) {
  if (!Object(owner, table, reason) || !Guard(!screens.contains(owner), "duplicate_screen_lifetime"))
    return;
  Screen screen;
  screen.id = ++next_identity;
  screens.emplace(owner, std::move(screen));
}

void CloseScreen(std::map<uint32_t, Screen>& screens, uint32_t owner, const char* source) {
  const auto found = screens.find(owner);
  if (found == screens.end())
    return;
  if (found->second.opened)
    Output().Enqueue(Event(source, "closed", found->second));
  screens.erase(found);
}

uint32_t TextOwner(TextScope scope, uint32_t record) {
  if (!record)
    return 0;
  if (scope == TextScope::PopupBody) {
    const auto found = record >= 0x74 ? popups.find(record - 0x74) : popups.end();
    return found != popups.end() && found->second.body.empty() ? found->first : 0;
  }
  if (scope == TextScope::MenuHints) {
    for (auto& [owner, screen] : popups) {
      if (screen.active && screen.text_root &&
          record == screen.text_root + 0x94) {
        text_scope = TextScope::PopupHints;
        return screen.hints.empty() ? owner : 0;
      }
    }
    for (auto& [owner, screen] : saves) {
      if (screen.active && screen.text_root &&
          record == screen.text_root + 0x94) {
        text_scope = TextScope::SaveHints;
        return screen.hints.empty() ? owner : 0;
      }
    }
    if (!popups.empty() || !saves.empty()) return 0;
  }
  if (scope == TextScope::TitlePrompt) {
    const auto found = record >= 0x20 ? titles.find(record - 0x20) : titles.end();
    if (found == titles.end()) {
      Guard(false, "title_text_record_owner");
      return 0;
    }
    return found->second.text.contains(0x9E5) ? 0 : found->first;
  }
  if (menus.empty() || !Guard(menus.size() == 1, "ambiguous_menu_text_owner"))
    return 0;
  const auto& [owner, screen] = *menus.begin();
  const bool hints = scope == TextScope::MenuHints;
  if ((hints && !screen.hints.empty()) ||
      (!hints && (!screen.verified || screen.text.contains(screen.description_id))))
    return 0;
  if (!screen.text_root ||
      !Guard(record == screen.text_root + (hints ? 0x94 : 0x60), "menu_text_record_owner"))
    return 0;
  return owner;
}

class ScopedText {
 public:
  explicit ScopedText(TextScope scope, uint32_t owner = 0)
      : previous_scope_(text_scope), previous_owner_(text_owner) {
    text_scope = scope;
    text_owner = owner;
  }
  ~ScopedText() { text_scope = previous_scope_; text_owner = previous_owner_; }
 private:
  TextScope previous_scope_;
  uint32_t previous_owner_;
};

void RemoveSaveWidgets(uint32_t owner) {
  std::erase_if(save_widgets, [owner](const auto& widget) { return widget.second.owner == owner; });
}

void RegisterSaveWidget(uint32_t widget, uint32_t row, uint32_t table, TextScope scope) {
  if (!building_save || !saves.contains(building_save) ||
      !Guard(row == 0, "save_widget_row") || !Object(widget, table, "save_widget_table"))
    return;
  save_widgets.insert_or_assign(widget, SaveWidget{building_save, scope});
}

uint32_t SaveTextOwner(uint32_t widget, TextScope scope) {
  const auto found = save_widgets.find(widget);
  if (found == save_widgets.end() || found->second.scope != scope)
    return 0;
  const auto screen = saves.find(found->second.owner);
  if (screen == saves.end() || !screen->second.active)
    return 0;
  return found->second.owner;
}

void CaptureDrawText(uint32_t pointer) {
  auto* screens = text_scope == TextScope::PopupBody || text_scope == TextScope::PopupHints
                  ? &popups : &saves;
  const auto found = screens->find(text_owner);
  if (!text_owner || found == screens->end()) return;
  auto& screen = found->second;
  uint32_t field = 0;
  switch (text_scope) {
    case TextScope::SaveName: field = 0; break;
    case TextScope::SaveDate: field = 0xE75; break;
    case TextScope::SaveTime: field = 0xE76; break;
    case TextScope::SaveEmpty: field = 0xA3F; break;
    case TextScope::PopupBody:
      if (!screen.body.empty()) return;
      break;
    case TextScope::SaveHints: case TextScope::PopupHints:
      if (!screen.hints.empty()) return;
      break;
    default: return;
  }
  if (screens == &saves && text_scope != TextScope::SaveHints && screen.text.contains(field))
    return;
  const auto text = memory.Text(pointer);
  if (!Guard(text.value.has_value(), brawlers::ui::ReadErrorName(text.error))) return;
  if (text_scope == TextScope::PopupBody)
    screen.body = *text.value;
  else if (text_scope == TextScope::SaveHints || text_scope == TextScope::PopupHints)
    screen.hints = *text.value;
  else
    screen.text.emplace(field, *text.value);
  if (screens == &popups) EmitPopup(screen);
  else EmitSave(screen);
}

void ForgetMovieDecoder(uint32_t codec) {
  const auto found = movie_decoders.find(codec);
  if (found == movie_decoders.end()) return;
  movie_states.erase(found->second.state);
  movie_decoders.erase(found);
  for (auto frame = movie_frames.begin(); frame != movie_frames.end();) {
    if (frame->second.codec == codec) frame = movie_frames.erase(frame);
    else ++frame;
  }
}

void CopyMoviePicture(uint32_t codec, uint32_t source, uint32_t destination) {
  const auto found = movie_decoders.find(codec);
  if (found == movie_decoders.end() || !destination) return;
  auto& pictures = found->second.pictures;
  const auto original = pictures.find(source);
  if (original != pictures.end()) pictures[destination] = original->second;
  else pictures.erase(destination);
}

}  // namespace

void SetGameSpeechReader(brawlers::ui::ReadBytes reader) {
  if (!Output().Start())
    throw std::runtime_error("game event output unavailable");
  memory = GuestUiMemory(std::move(reader));
}

bool FlushGameSpeech() { return Output().Flush(); }

void BrawlersMainMenuStateChanged(PPCRegister& owner, PPCRegister& state) {
  Observe([&] { MainState(owner.u32, state.u32); });
}

void BrawlersMainMenuSelectionChanged(PPCRegister& owner, PPCRegister& selected,
                                     PPCRegister& description) {
  Observe([&] { MainSelection(owner.u32, selected.u32, description.u32); });
}

void BrawlersTitleStateChanged(PPCRegister& owner, PPCRegister& state) {
  Observe([&] { TitleState(owner.u32, state.u32); });
}

extern "C" void sub_822CED20(PPCContext& ctx, uint8_t* base) {
  const auto owner = ctx.r3.u32;
  __imp__sub_822CED20(ctx, base);
  Observe([&] { NewScreen(menus, owner, 0x8201205C, "menu_constructor"); });
}

extern "C" void sub_822CF058(PPCContext& ctx, uint8_t* base) {
  const auto owner = ctx.r3.u32;
  __imp__sub_822CF058(ctx, base);
  Observe([&] {
    const auto found = menus.find(owner);
    if (found == menus.end() || !Object(owner, 0x8201205C, "menu_setup_owner"))
      return;
    const auto loader = memory.U32BE(owner + 0xC4);
    if (!Guard(loader.has_value() && *loader && *loader <= UINT32_MAX - 0x22,
               "menu_resource_loader"))
      return;
    if (!Guard(memory.U32BE(*loader) == 0x82003F2C, "menu_resource_loader_table"))
      return;
    const auto resource = memory.U16BE(*loader + 0x20);
    if (!Guard(resource.has_value() && *resource >= 0x8E4 && *resource <= 0x8EA,
               "menu_resource_locale"))
      return;
    found->second.locale = *resource - 0x8E3;
    const auto root = memory.U32BE(0x829CBC68);
    if (Guard(root.has_value() && *root && *root <= UINT32_MAX - 0x94,
              "menu_text_global"))
      found->second.text_root = *root;
  });
}

extern "C" void sub_822DA190(PPCContext& ctx, uint8_t* base) {
  const auto owner = ctx.r3.u32;
  __imp__sub_822DA190(ctx, base);
  Observe([&] { NewScreen(titles, owner, 0x8201244C, "title_constructor"); });
}

extern "C" void sub_821E9C30(PPCContext& ctx, uint8_t* base) {
  ScopedText scope(TextScope::MenuDescription);
  __imp__sub_821E9C30(ctx, base);
}

extern "C" void sub_822DADA0(PPCContext& ctx, uint8_t* base) {
  ScopedText scope(TextScope::TitlePrompt);
  __imp__sub_822DADA0(ctx, base);
}

extern "C" void sub_821EA650(PPCContext& ctx, uint8_t* base) {
  ScopedText scope(TextScope::MenuHints);
  __imp__sub_821EA650(ctx, base);
}

extern "C" void sub_823B60B8(PPCContext& ctx, uint8_t* base) {
  const auto scope = text_scope;
  const auto owner = text_owner;
  const auto caller = ctx.lr;
  const auto bank = ctx.r5.u8;
  const auto id = ctx.r4.u32;
  __imp__sub_823B60B8(ctx, base);
  if (scope == TextScope::None || !owner || bank != 0 ||
      (caller != 0x823A9958 && caller != 0x823AA6C4))
    return;
  const auto pointer = ctx.r3.u32;
  Observe([&] {
    const bool title = scope == TextScope::TitlePrompt;
    const bool hints = scope == TextScope::MenuHints;
    if ((title && id != 0x9E5) || (hints && id != 0x9D6 && id != 0x9EC) ||
        (!title && !hints && id != 0xA43 && (id < 0x613 || id > 0x617)))
      return;
    auto& screens = title ? titles : menus;
    const auto found = screens.find(owner);
    if (found == screens.end())
      return;
    auto& screen = found->second;
    if ((!title && !hints && id != screen.description_id) ||
        (!hints && screen.text.contains(id)) || (hints && !screen.hints.empty()))
      return;
    const auto text = memory.Text(pointer);
    if (!Guard(text.value.has_value(), brawlers::ui::ReadErrorName(text.error)))
      return;
    if (hints)
      screen.hints = *text.value;
    else
      screen.text.emplace(id, *text.value);
    if (title)
      EmitTitle(screen);
    else
      EmitMenu(screen);
  });
}

extern "C" void sub_823B87A8(PPCContext& ctx, uint8_t* base) {
  const auto owner = ctx.r3.u32;
  __imp__sub_823B87A8(ctx, base);
  Observe([&] {
    CloseScreen(menus, owner, "menu");
    CloseScreen(titles, owner, "title");
    CloseScreen(popups, owner, "popup");
    CloseScreen(saves, owner, "save");
    RemoveSaveWidgets(owner);
  });
}

extern "C" void sub_822CFDF8(PPCContext& ctx, uint8_t* base) {
  const auto owner = ctx.r3.u32;
  __imp__sub_822CFDF8(ctx, base);
  Observe([&] { CloseScreen(menus, owner, "menu"); });
}

extern "C" void sub_822DAD58(PPCContext& ctx, uint8_t* base) {
  const auto owner = ctx.r3.u32;
  __imp__sub_822DAD58(ctx, base);
  Observe([&] { CloseScreen(titles, owner, "title"); });
}

// These ranges identify actual compressed pictures, verified against all four
// supplied videos. Decoder events preserve their identity through reordering.
void BrawlersMovieRendered(PPCRegister& result, PPCRegister& player, PPCRegister& frame) {
  if (result.u32 != 0 || !drawing_movie) return;
  Observe([&] {
    const auto found = logos.find(drawing_movie);
    const auto queued = movie_frames.find({player.u32, frame.u32});
    if (found == logos.end() || !found->second.verified || found->second.opened ||
        queued == movie_frames.end()) return;
    const auto decoder = movie_decoders.find(queued->second.codec);
    if (decoder == movie_decoders.end() ||
        decoder->second.generation != queued->second.generation ||
        decoder->second.generation < found->second.decoder_generation_floor) return;
    constexpr std::array<std::pair<uint32_t, uint32_t>, 4> cues{{
        {177, 280}, {48, 175}, {23, 170}, {0, 210}}};
    auto& screen = found->second;
    if (screen.selection >= cues.size()) return;
    const auto [first, last] = cues[screen.selection];
    if (queued->second.object < first || queued->second.object > last) return;
    auto event = Event("logo", "opened", screen);
    event.title_key = "logo_" + std::to_string(screen.selection);
    Output().Enqueue(std::move(event));
    screen.opened = true;
  });
}

void BrawlersMovieDecoderReady(PPCRegister& state) {
  Observe([&] {
    if (!Guard(state.u32 && state.u32 <= UINT32_MAX - 0x44, "movie_codec_state")) return;
    const auto previous = movie_states.find(state.u32);
    if (previous != movie_states.end()) ForgetMovieDecoder(previous->second);
    const auto codec = memory.U32BE(state.u32 + 0x44);
    if (!Guard(codec.has_value() && *codec && *codec <= UINT32_MAX - 0xE80,
               "movie_codec_owner")) return;
    ForgetMovieDecoder(*codec);
    MovieDecoder decoder;
    decoder.state = state.u32;
    decoder.generation = ++next_movie_generation;
    movie_decoders.emplace(*codec, std::move(decoder));
    movie_states.emplace(state.u32, *codec);
  });
}

void BrawlersMovieDecoderInvalidated(PPCRegister& codec) {
  Observe([&] { ForgetMovieDecoder(codec.u32); });
}

void BrawlersMovieObject(PPCRegister& state) {
  Observe([&] {
    const auto owner = movie_states.find(state.u32);
    if (owner == movie_states.end()) return;
    auto& decoder = movie_decoders.at(owner->second);
    if (!Guard(decoder.next_object <= UINT32_MAX, "movie_object_overflow")) {
      ForgetMovieDecoder(owner->second);
      return;
    }
    decoder.current = static_cast<uint32_t>(decoder.next_object++);
  });
}

void BrawlersMoviePictureStarted(PPCRegister& codec) {
  Observe([&] {
    const auto found = movie_decoders.find(codec.u32);
    if (found != movie_decoders.end()) found->second.started = found->second.current;
  });
}

void BrawlersMoviePictureCommitted(PPCRegister& codec) {
  Observe([&] {
    const auto found = movie_decoders.find(codec.u32);
    if (found == movie_decoders.end()) return;
    auto& decoder = found->second;
    const auto picture = memory.U32BE(codec.u32 + 0xE80);
    if (!Guard(picture.has_value() && *picture, "movie_committed_picture")) {
      ForgetMovieDecoder(codec.u32);
      return;
    }
    if (decoder.started) decoder.pictures[*picture] = *decoder.started;
    else decoder.pictures.erase(*picture);
  });
}

void BrawlersMoviePictureCopied(PPCRegister& codec, PPCRegister& source,
                                PPCRegister& destination) {
  Observe([&] { CopyMoviePicture(codec.u32, source.u32, destination.u32); });
}

void BrawlersMoviePictureProcessed(PPCRegister& codec, PPCRegister& destination) {
  Observe([&] {
    if (!movie_decoders.contains(codec.u32)) return;
    const auto source = memory.U32BE(codec.u32 + 0xE80);
    if (!Guard(source.has_value() && *source, "movie_processed_source")) {
      ForgetMovieDecoder(codec.u32);
      return;
    }
    CopyMoviePicture(codec.u32, *source, destination.u32);
  });
}

void BrawlersMoviePictureSelected(PPCRegister& codec, PPCRegister& picture,
                                  PPCRegister& destination) {
  if (!movie_output.player || !destination.u32) return;
  Observe([&] {
    movie_output.selected.reset();
    const auto found = movie_decoders.find(codec.u32);
    if (found == movie_decoders.end()) return;
    const auto selected = found->second.pictures.find(picture.u32);
    if (selected != found->second.pictures.end())
      movie_output.selected = MoviePicture{codec.u32, found->second.generation, selected->second};
  });
}

void BrawlersMovieDecodeBegin(PPCRegister& player, PPCRegister& frame,
                              PPCRegister& capacity, PPCRegister& byte_count) {
  Observe([&] {
    movie_output = {};
    movie_frames.erase({player.u32, frame.u32});
    // The four verified logo streams all produce 1280 by 720 planar YUV420.
    if (player.u32 && frame.u32 && capacity.u32 == 1280 * 720 * 3 / 2)
      movie_output = MovieOutput{player.u32, frame.u32, byte_count.u32, std::nullopt};
  });
}

void BrawlersMovieDecoded(PPCRegister& result, PPCRegister& player, PPCRegister& frame) {
  Observe([&] {
    const auto pending = movie_output;
    movie_output = {};
    if (pending.player != player.u32 || pending.frame != frame.u32 ||
        result.s32 < 0 || !pending.selected) return;
    const auto count = memory.U32BE(pending.byte_count);
    if (!Guard(count.has_value(), "movie_output_byte_count") ||
        *count != 1280 * 720 * 3 / 2) return;
    const auto decoder = movie_decoders.find(pending.selected->codec);
    if (decoder != movie_decoders.end() &&
        decoder->second.generation == pending.selected->generation)
      movie_frames[{player.u32, frame.u32}] = *pending.selected;
  });
}

extern "C" void sub_823AFF38(PPCContext& ctx, uint8_t* base) {
  const auto owner = ctx.r3.u32;
  __imp__sub_823AFF38(ctx, base);
  Observe([&] { CloseScreen(logos, owner, "logo"); });
}

void BrawlersMovieSetup(PPCRegister& owner, PPCRegister& movie) {
  Observe([&] {
    const auto pending = logos.find(owner.u32);
    if (pending != logos.end()) {
      if (pending->second.selection != movie.u32)
        pending->second.verified = false;
      return;
    }
    if (movie.u32 > 3 ||
        !Object(owner.u32, 0x8203608C, "movie_setup_table"))
      return;
    Screen screen;
    screen.id = ++next_identity;
    screen.selection = movie.u32;
    screen.decoder_generation_floor = next_movie_generation + 1;
    screen.verified = true;
    logos.emplace(owner.u32, std::move(screen));
  });
}

void BrawlersMovieDraw(PPCRegister& owner, PPCRegister& wrapper) {
  Observe([&] {
    drawing_movie = 0;
    const auto found = logos.find(owner.u32);
    if (found != logos.end() && found->second.verified && !found->second.opened &&
        Guard(wrapper.u32 != 0, "movie_draw_wrapper"))
      drawing_movie = owner.u32;
  });
}

void BrawlersMovieClosed(PPCRegister& owner) {
  Observe([&] { CloseScreen(logos, owner.u32, "logo"); });
}

extern "C" void sub_823AFCB8(PPCContext& ctx, uint8_t* base) {
  struct DrawScope {
    uint32_t previous = drawing_movie;
    DrawScope() { drawing_movie = 0; }
    ~DrawScope() { drawing_movie = previous; }
  } scope;
  __imp__sub_823AFCB8(ctx, base);
}

void BrawlersTextRecord(PPCRegister& record) {
  if (text_scope == TextScope::None)
    return;
  Observe([&] { text_owner = TextOwner(text_scope, record.u32); });
}

void BrawlersNativeSelectionChanged(PPCRegister& selector, PPCRegister& selected) {
  Observe([&] {
    if (selector.u32 >= 0x18) {
      const auto popup = popups.find(selector.u32 - 0x18);
      if (popup != popups.end()) {
        auto& screen = popup->second;
        screen.focused = Guard(selected.u32 < screen.total, "popup_selection_event");
        if (screen.focused) screen.selection = selected.u32;
        EmitPopup(screen);
        return;
      }
    }
    const auto save = selector.u32 >= 0x20 ? saves.find(selector.u32 - 0x20) : saves.end();
    if (save != saves.end()) {
      auto& screen = save->second;
      screen.focused = Guard(selected.u32 == 0, "save_selection_event");
      if (screen.focused) screen.selection = selected.u32;
      EmitSave(screen);
    }
  });
}

void BrawlersPopupStateChanged(PPCRegister& owner, PPCRegister& state) {
  Observe([&] {
    const auto found = popups.find(owner.u32);
    if (found == popups.end()) return;
    auto& screen = found->second;
    screen.active = state.u32 == 2 && Guard(
        memory.U32BE(owner.u32) == (screen.total == 1 ? 0x82003C90 : 0x82003C80),
        "popup_active_table");
    if (screen.active) EmitPopup(screen);
  });
}

void BrawlersSaveStateChanged(PPCRegister& owner, PPCRegister& state) {
  Observe([&] {
    const auto found = saves.find(owner.u32);
    if (found == saves.end()) return;
    auto& screen = found->second;
    screen.active = state.u32 == 4;
    if (!screen.active) return;
    screen.text.clear();
    screen.hints.clear();
    screen.resume = screen.opened;
    if (!screen.opened) {
      Output().Enqueue(Event("save", "opened", screen));
      screen.opened = true;
    }
  });
}

extern "C" void sub_822D38D0(PPCContext& ctx, uint8_t* base) {
  const auto owner = ctx.r3.u32, mode = ctx.r6.u32;
  __imp__sub_822D38D0(ctx, base);
  Observe([&] {
    if (mode != 0) return;
    NewScreen(saves, owner, 0x820121DC, "save_constructor");
    const auto found = saves.find(owner);
    if (found != saves.end()) {
      found->second.total = 1;
      CommonResource(found->second);
    }
  });
}

extern "C" void sub_822D30D8(PPCContext& ctx, uint8_t* base) {
  const auto owner = ctx.r3.u32;
  Observe([&] { RemoveSaveWidgets(owner); CloseScreen(saves, owner, "save"); });
  __imp__sub_822D30D8(ctx, base);
}

extern "C" void sub_822D3830(PPCContext& ctx, uint8_t* base) {
  const auto owner = ctx.r3.u32;
  Observe([&] { RemoveSaveWidgets(owner); });
  __imp__sub_822D3830(ctx, base);
}

extern "C" void sub_822D3A50(PPCContext& ctx, uint8_t* base) {
  struct Scope {
    uint32_t previous = building_save;
    explicit Scope(uint32_t owner) { building_save = owner; }
    ~Scope() { building_save = previous; }
  } scope(ctx.r3.u32);
  __imp__sub_822D3A50(ctx, base);
}

#define SAVE_WIDGET(address, table, field) \
  extern "C" void sub_##address(PPCContext& ctx, uint8_t* base) { \
    const auto widget = ctx.r3.u32, row = ctx.r5.u32; \
    __imp__sub_##address(ctx, base); \
    Observe([&] { RegisterSaveWidget(widget, row, table, TextScope::field); }); \
  }
SAVE_WIDGET(822D6688, 0x82012224, SaveDate)
SAVE_WIDGET(822D6238, 0x8201221C, SaveTime)
SAVE_WIDGET(822D5F48, 0x82012214, SaveName)
SAVE_WIDGET(822D6AD8, 0x8201222C, SaveEmpty)
#undef SAVE_WIDGET

#define SAVE_DRAW(address, field) \
  extern "C" void sub_##address(PPCContext& ctx, uint8_t* base) { \
    uint32_t owner = 0; \
    Observe([&] { owner = SaveTextOwner(ctx.r3.u32, TextScope::field); }); \
    ScopedText scope(TextScope::field, owner); \
    __imp__sub_##address(ctx, base); \
  }
SAVE_DRAW(822D6798, SaveDate)
SAVE_DRAW(822D6368, SaveTime)
SAVE_DRAW(822D6050, SaveName)
SAVE_DRAW(822D6BD0, SaveEmpty)
#undef SAVE_DRAW

extern "C" void sub_821DFBE8(PPCContext& ctx, uint8_t* base) {
  const auto owner = ctx.r3.u32;
  const auto kind = ctx.r4.u8;
  const auto bank = ctx.r6.u32, body = ctx.r7.u32;
  __imp__sub_821DFBE8(ctx, base);
  Observe([&] {
    if (kind > 1) return;
    NewScreen(popups, owner, 0x82003C70, "popup_constructor");
    const auto found = popups.find(owner);
    if (found == popups.end()) return;
    found->second.total = kind == 0 ? 2 : 1;
    found->second.bank = bank;
    found->second.description_id = body;
    CommonResource(found->second);
  });
}

extern "C" void sub_821DF478(PPCContext& ctx, uint8_t* base) {
  const auto owner = ctx.r3.u32;
  Observe([&] { CloseScreen(popups, owner, "popup"); });
  __imp__sub_821DF478(ctx, base);
}

extern "C" void sub_821E1790(PPCContext& ctx, uint8_t* base) {
  ScopedText scope(TextScope::PopupBody);
  __imp__sub_821E1790(ctx, base);
}

extern "C" void sub_823A9528(PPCContext& ctx, uint8_t* base) {
  const auto pointer = ctx.r4.u32;
  if (text_owner && ((text_scope == TextScope::SaveName && ctx.lr == 0x822D61C8) ||
      (text_scope != TextScope::SaveName && ctx.lr == 0x823A9A24) ||
      ((text_scope == TextScope::SaveHints || text_scope == TextScope::PopupHints) &&
       ctx.lr == 0x821EACD8)))
    Observe([&] { CaptureDrawText(pointer); });
  __imp__sub_823A9528(ctx, base);
}

extern "C" void sub_823A9908(PPCContext& ctx, uint8_t* base) {
  if (!text_owner || text_scope == TextScope::None || text_scope == TextScope::MenuDescription ||
      text_scope == TextScope::MenuHints || text_scope == TextScope::TitlePrompt) {
    __imp__sub_823A9908(ctx, base);
    return;
  }
  bool accepted = true;
  const auto bank = ctx.r4.u32, id = ctx.r5.u32;
  Observe([&] {
    switch (text_scope) {
      case TextScope::SaveDate: accepted = bank == 0 && id == 0xE75; break;
      case TextScope::SaveTime: accepted = bank == 0 && id == 0xE76; break;
      case TextScope::SaveEmpty: accepted = bank == 0 && id == 0xA3F; break;
      case TextScope::PopupBody: {
        const auto popup = popups.find(text_owner);
        accepted = popup != popups.end() && bank == popup->second.bank &&
                   id == popup->second.description_id;
        break;
      }
      case TextScope::SaveHints: accepted = bank == 0 && (id == 0x9F6 || id == 0x9D6); break;
      case TextScope::PopupHints: accepted = bank == 0 && id == 0xA16; break;
      default: break;
    }
  });
  ScopedText scope(text_scope, accepted ? text_owner : 0);
  __imp__sub_823A9908(ctx, base);
}

extern "C" void sub_823A9A38(PPCContext& ctx, uint8_t* base) {
  const auto pointer = ctx.r4.u32;
  if (text_owner && (text_scope == TextScope::SaveHints || text_scope == TextScope::PopupHints) &&
      (ctx.lr == 0x823AA75C || ctx.lr == 0x823AA2B0))
    Observe([&] { CaptureDrawText(pointer); });
  __imp__sub_823A9A38(ctx, base);
}

extern "C" void sub_821DF130(PPCContext& ctx, uint8_t* base) {
  const auto selector = ctx.r3.u32, lower = ctx.r4.u32, upper = ctx.r5.u32;
  __imp__sub_821DF130(ctx, base);
  Observe([&] {
    Screen* screen = nullptr;
    const auto popup = selector >= 0x18 ? popups.find(selector - 0x18) : popups.end();
    const auto save = selector >= 0x20 ? saves.find(selector - 0x20) : saves.end();
    if (popup != popups.end()) screen = &popup->second;
    else if (save != saves.end()) screen = &save->second;
    if (screen) screen->verified = Guard(lower == 0 && upper == screen->total,
                                         "native_list_bounds");
  });
}
