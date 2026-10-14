// Original synthetic lifecycle fixtures; no translated game code is executed.
#include "native_game_ui.h"
#include "native_speech.h"

#include <cassert>
#include <array>
#include <cfenv>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <vector>
#include <rex/ppc/context.h>
#include <rex/thread/mutex.h>

#define HOOK(address) extern "C" void sub_##address(PPCContext&, uint8_t*)
HOOK(822CED20); HOOK(822CF058); HOOK(822DA190);
HOOK(821E9C30); HOOK(821EA650); HOOK(822DADA0); HOOK(823B60B8); HOOK(823B87A8);
HOOK(823AFCB8); HOOK(823AFF38); HOOK(822CFDF8); HOOK(822DAD58);
HOOK(822D38D0);
HOOK(822D30D8);
HOOK(822D3A50);
HOOK(822D3830);
HOOK(822D6688);
HOOK(822D6238);
HOOK(822D5F48);
HOOK(822D6AD8);
HOOK(822D6798);
HOOK(822D6368);
HOOK(822D6050);
HOOK(822D6BD0);
HOOK(821DFBE8);
HOOK(821DF478);
HOOK(821E1790);
HOOK(823A9528);
HOOK(823A9908);
HOOK(823A9A38);
HOOK(821DF130);
#undef HOOK
void BrawlersMainMenuStateChanged(PPCRegister&, PPCRegister&);
void BrawlersMainMenuSelectionChanged(PPCRegister&, PPCRegister&, PPCRegister&);
void BrawlersTitleStateChanged(PPCRegister&, PPCRegister&);
void BrawlersMovieSetup(PPCRegister&, PPCRegister&);
void BrawlersMovieDraw(PPCRegister&, PPCRegister&);
void BrawlersMovieClosed(PPCRegister&);
void BrawlersMovieRendered(PPCRegister&, PPCRegister&, PPCRegister&);
void BrawlersMovieDecoderReady(PPCRegister&);
void BrawlersMovieDecoderInvalidated(PPCRegister&);
void BrawlersMovieObject(PPCRegister&);
void BrawlersMoviePictureStarted(PPCRegister&);
void BrawlersMoviePictureCommitted(PPCRegister&);
void BrawlersMoviePictureProcessed(PPCRegister&, PPCRegister&);
void BrawlersMoviePictureCopied(PPCRegister&, PPCRegister&, PPCRegister&);
void BrawlersMoviePictureSelected(PPCRegister&, PPCRegister&, PPCRegister&);
void BrawlersMovieDecodeBegin(PPCRegister&, PPCRegister&, PPCRegister&, PPCRegister&);
void BrawlersMovieDecoded(PPCRegister&, PPCRegister&, PPCRegister&);

void BrawlersTextRecord(PPCRegister&);
void BrawlersNativeSelectionChanged(PPCRegister&, PPCRegister&);
void BrawlersPopupStateChanged(PPCRegister&, PPCRegister&);
void BrawlersSaveStateChanged(PPCRegister&, PPCRegister&);

// Use the actual recursive-lock contract without initializing runtime services.
std::recursive_mutex& rex::thread::global_critical_region::mutex() {
  static std::recursive_mutex lock;
  return lock;
}

namespace {
std::map<uint32_t, uint8_t> bytes;
std::vector<ScreenEvent> events;
std::map<uint32_t, std::function<void(PPCContext&)>> actions;
unsigned originals = 0, reads = 0;
uint8_t opaque[1]{};

void Word(uint32_t address, uint32_t value) {
  for (unsigned i = 0; i < 4; ++i)
    bytes[address + i] = static_cast<uint8_t>(value >> ((3 - i) * 8));
}
void Text(uint32_t address, const char16_t* value) {
  do {
    bytes[address++] = static_cast<uint8_t>(*value >> 8);
    bytes[address++] = static_cast<uint8_t>(*value);
  } while (*value++);
}
void Original(uint32_t address, PPCContext& ctx, uint8_t* base) {
  assert(base == opaque);
  ++originals;
  if (actions.contains(address)) actions.at(address)(ctx);
  ctx.r7.u64 = 0xabcde0123456789;
  ctx.f1.f64 = 0.5;
  std::fesetround(FE_DOWNWARD);
  std::feclearexcept(FE_ALL_EXCEPT);
  std::feraiseexcept(FE_INVALID);
}
void Call(void (*hook)(PPCContext&, uint8_t*), uint32_t owner) {
  PPCContext ctx{};
  ctx.r3.u32 = owner;
  ctx.r8.u64 = 0x9876543210;
  ctx.f2.f64 = 0.125;
  ctx.lr = 0x12345678;
  hook(ctx, opaque);
  assert(ctx.r7.u64 == 0xabcde0123456789 && ctx.f1.f64 == 0.5);
  assert(ctx.r8.u64 == 0x9876543210 && ctx.f2.f64 == 0.125 && ctx.lr == 0x12345678);
  assert(std::fegetround() == FE_DOWNWARD);
  assert(std::fetestexcept(FE_ALL_EXCEPT) == FE_INVALID);
  assert(FlushGameSpeech());
}
// Each register event must preserve its complete register arguments and host FP state.
template <typename Function, typename... Values>
void Event(Function function, Values... values) {
  std::array<PPCRegister, sizeof...(values)> args{};
  size_t i = 0;
  ((args[i++].u64 = values), ...);
  const auto before = args;
  std::fesetround(FE_UPWARD);
  std::feclearexcept(FE_ALL_EXCEPT);
  std::feraiseexcept(FE_DIVBYZERO);
  [&]<size_t... N>(std::index_sequence<N...>) { function(args[N]...); }
    (std::make_index_sequence<sizeof...(values)>{});
  assert(std::memcmp(args.data(), before.data(), sizeof(args)) == 0);
  assert(std::fegetround() == FE_UPWARD);
  assert(std::fetestexcept(FE_ALL_EXCEPT) == FE_DIVBYZERO);
  assert(FlushGameSpeech());
}
void Capture(uint32_t id, const char16_t* text, void (*scope)(PPCContext&, uint8_t*),
             uint32_t record, uint32_t caller = 0x823A9958) {
  Text(0x4000, text);
  actions[0x823B60B8] = [](PPCContext& ctx) { ctx.r3.u32 = 0x4000; };
  const auto call = [=](PPCContext&) {
    PPCRegister native_record{};
    native_record.u32 = record;
    BrawlersTextRecord(native_record);
    PPCContext lookup{};
    lookup.r4.u32 = id;
    lookup.lr = caller;
    sub_823B60B8(lookup, opaque);
    assert(lookup.r3.u32 == 0x4000);
  };
  actions[0x821E9C30] = call;
  actions[0x821EA650] = call;
  actions[0x822DADA0] = call;
  // Widget/context need not be mapped: original record-load event supplies identity.
  Call(scope, 0xDEAD);
}
}  // namespace

bool SendScreenEvent(const ScreenEvent& event) noexcept { events.push_back(event); return true; }

#define STUB(address) \
  extern "C" void __imp__sub_##address(PPCContext& ctx, uint8_t* base) { \
    Original(0x##address, ctx, base); \
  }
STUB(822CED20) STUB(822CF058) STUB(822DA190)
STUB(821E9C30) STUB(821EA650) STUB(822DADA0) STUB(823B60B8) STUB(823B87A8)
STUB(823AFCB8) STUB(823AFF38) STUB(822CFDF8) STUB(822DAD58)
STUB(822D38D0)
STUB(822D30D8)
STUB(822D3A50)
STUB(822D3830)
STUB(822D6688)
STUB(822D6238)
STUB(822D5F48)
STUB(822D6AD8)
STUB(822D6798)
STUB(822D6368)
STUB(822D6050)
STUB(822D6BD0)
STUB(821DFBE8)
STUB(821DF478)
STUB(821E1790)
STUB(823A9528)
STUB(823A9908)
STUB(823A9A38)
STUB(821DF130)
#undef STUB

int main() {
  SetGameSpeechReader([](uint32_t address, void* output, size_t size) {
    ++reads;
    auto* destination = static_cast<uint8_t*>(output);
    for (size_t i = 0; i < size; ++i) {
      const auto found = bytes.find(address + static_cast<uint32_t>(i));
      if (found == bytes.end()) return false;
      destination[i] = found->second;
    }
    return true;
  });
  constexpr uint32_t menu = 0x1000, title = 0x6000, movie = 0x7000;
  Word(menu, 0x8201205C); Word(menu + 0xC4, 0x2000); Word(0x2000, 0x82003F2C);
  bytes[0x2020] = 8; bytes[0x2021] = 0xE4;
  Word(0x829CBC68, 0xA000);
  Call(sub_822CED20, menu); Call(sub_822CF058, menu);
  assert(events.empty());
  const auto setup_reads = reads;
  Event(BrawlersMainMenuStateChanged, menu, 4);
  assert(reads == setup_reads && events.size() == 1 && events.back().kind == "opened");
  Event(BrawlersMainMenuSelectionChanged, menu, 0, 0x613);
  Capture(0x613, u"Wrong owner", sub_821E9C30, 0xDEAD);
  Capture(0x613, u"Measurement only", sub_821E9C30, 0xA060, 0x11111111);
  assert(events.size() == 1);
  Capture(0x613, u"Synthetic first description", sub_821E9C30, 0xA060, 0x823AA6C4);
  assert(events.size() == 2 && events.back().label_key == "main_0");
  assert(events.back().position == 1 && events.back().total == 5 && events.back().locale == 1);
  const auto menu_identity = events.back().id;
  Capture(0x9D6, u"Synthetic hint", sub_821EA650, 0xA094);
  assert(events.size() == 3 && events.back().hints.size() == 1);
  const auto cached_reads = reads;
  for (unsigned i = 0; i < 10; ++i) {
    Capture(0x613, u"Already captured", sub_821E9C30, 0xA060);
    Capture(0x9D6, u"Already captured", sub_821EA650, 0xA094);
  }
  assert(reads == cached_reads && events.size() == 3);
  Event(BrawlersMainMenuSelectionChanged, menu, 99, 0x613);
  Capture(0x613, u"Invalid selection", sub_821E9C30, 0xA060);
  assert(events.size() == 3);
  Event(BrawlersMainMenuSelectionChanged, menu, 3, 0xA43);
  Capture(0x616, u"Wrong enabled description", sub_821E9C30, 0xA060);
  assert(events.size() == 3);
  Capture(0xA43, u"Synthetic unavailable description", sub_821E9C30, 0xA060);
  assert(events.size() == 4 && events.back().position == 4 && events.back().total == 5);
  Event(BrawlersMainMenuStateChanged, menu, 5);
  Event(BrawlersMainMenuSelectionChanged, menu, 0, 0x613);
  assert(events.size() == 4); // Beginning exit is not a close or a child return.
  Call(sub_822CFDF8, menu); Call(sub_823B87A8, menu);
  assert(events.size() == 5 && events.back().kind == "closed");
  Call(sub_822CED20, menu); Call(sub_822CF058, menu);
  Event(BrawlersMainMenuStateChanged, menu, 4);
  assert(events.back().kind == "opened" && events.back().id != menu_identity);
  Call(sub_823B87A8, menu);

  Word(title, 0x8201244C);
  Call(sub_822DA190, title);
  const auto before_title = events.size();
  Event(BrawlersTitleStateChanged, title, 3);
  assert(events.size() == before_title + 1 && events.back().title_key == "title");
  Event(BrawlersTitleStateChanged, title, 6);
  Capture(0x9E5, u"Synthetic title prompt", sub_822DADA0, title + 0x20);
  assert(events.size() == before_title + 1);
  Event(BrawlersTitleStateChanged, title, 3);
  assert(events.back().kind == "resumed" && events.back().label == "Synthetic title prompt");
  for (auto state : {9, 13}) {
    Event(BrawlersTitleStateChanged, title, state);
    Event(BrawlersTitleStateChanged, title, 3);
    assert(events.back().kind == "resumed" && events.back().title.empty());
  }
  Event(BrawlersTitleStateChanged, title, 4);
  Call(sub_822DAD58, title);
  const auto closed_title = events.size();
  Call(sub_823B87A8, title);
  assert(events.size() == closed_title && events.back().kind == "closed");

  Word(movie, 0x8203608C);
  constexpr uint32_t codec_state = 0x100000, codec = 0x110000;
  constexpr uint32_t picture = 0x120000, processed = 0x120100;
  constexpr uint32_t player = 0x130000, frame = 0x140000, output_bytes = 0x150000;
  constexpr uint32_t image_bytes = 1280 * 720 * 3 / 2;
  Word(codec_state + 0x44, codec); Word(codec + 0xE80, picture);
  Word(output_bytes, image_bytes);
  uint32_t render_result = 0;
  actions[0x823AFCB8] = [&](PPCContext& ctx) {
    Event(BrawlersMovieDraw, ctx.r3.u32, 0x1234);
    Event(BrawlersMovieRendered, render_result, player, frame);
  };
  const auto ready_picture = [&](uint32_t object, bool lookahead = false) {
    Event(BrawlersMovieDecoderReady, codec_state);
    Event(BrawlersMovieDecodeBegin, player, frame, image_bytes, output_bytes);
    for (uint32_t i = 0; i <= object; ++i) Event(BrawlersMovieObject, codec_state);
    Event(BrawlersMoviePictureStarted, codec);
    if (lookahead) Event(BrawlersMovieObject, codec_state);
    Event(BrawlersMoviePictureCommitted, codec);
    Event(BrawlersMoviePictureProcessed, codec, processed);
    Event(BrawlersMoviePictureSelected, codec, processed, 0x160000);
  };
  const auto publish = [&] { Event(BrawlersMovieDecoded, 0, player, frame); };
  const auto draw = [&] { Call(sub_823AFCB8, movie); };
  const std::array<std::pair<uint32_t, uint32_t>, 4> ranges{{
      {177, 280}, {48, 175}, {23, 170}, {0, 210}}};
  for (uint32_t index = 0; index < ranges.size(); ++index) {
    const auto before = events.size();
    Event(BrawlersMovieSetup, movie, index);
    draw(); // No queued identity: setup/readiness alone cannot announce.
    assert(events.size() == before);
    const auto [first, last] = ranges[index];
    ready_picture(last + 1); publish(); draw();
    assert(events.size() == before); // Lettering has disappeared.
    if (first) {
      ready_picture(first - 1, true); publish(); draw();
      assert(events.size() == before); // Input lookahead cannot advance the cue.
    }
    ready_picture(first, true); publish();
    Event(BrawlersMovieRendered, 0, player, frame); // No manager draw scope.
    assert(events.size() == before);
    render_result = 0x8000000A; draw();
    assert(events.size() == before); // Renderer retry does not consume identity.
    render_result = 0; draw();
    assert(events.size() == before + 1 &&
           events.back().title_key == "logo_" + std::to_string(index));
    const auto cached_movie_reads = reads;
    draw();
    assert(reads == cached_movie_reads && events.size() == before + 1);
    Event(BrawlersMovieDecoderInvalidated, codec);
    Event(BrawlersMovieClosed, movie); Call(sub_823AFF38, movie);
    assert(events.size() == before + 2 && events.back().kind == "closed");
  }
  Event(BrawlersMovieSetup, movie, 0);
  const auto before_movie_faults = events.size();
  ready_picture(177); Event(BrawlersMovieDecoded, 0x80000001, player, frame); draw();
  assert(events.size() == before_movie_faults);
  ready_picture(177); Word(output_bytes, 0); publish(); draw();
  assert(events.size() == before_movie_faults); // EOF is not an image.
  Word(output_bytes, image_bytes);
  ready_picture(177); publish();
  Event(BrawlersMovieDecodeBegin, player, frame, image_bytes, output_bytes);
  Event(BrawlersMoviePictureSelected, codec, picture, 0); publish(); draw();
  assert(events.size() == before_movie_faults); // Internal discard cannot reuse a slot.
  ready_picture(177); publish(); Event(BrawlersMovieDecoderInvalidated, codec); draw();
  assert(events.size() == before_movie_faults); // Seek/destruction invalidates queued identities.
  ready_picture(177); publish();
  Word(codec_state + 0x44, 0); Event(BrawlersMovieDecoderReady, codec_state); draw();
  assert(events.size() == before_movie_faults); // Failed replacement clears old ownership.
  Word(codec_state + 0x44, codec);
  ready_picture(176); publish();
  // Preserve one generation while the decoder advances to a new picture.
  Event(BrawlersMovieDecodeBegin, player, frame, image_bytes, output_bytes);
  Event(BrawlersMovieObject, codec_state);
  Event(BrawlersMoviePictureStarted, codec);
  Word(codec + 0xE80, picture + 0x200);
  Event(BrawlersMoviePictureCommitted, codec);
  Event(BrawlersMoviePictureCopied, codec, processed, processed + 0x200);
  Event(BrawlersMoviePictureProcessed, codec, processed);
  Event(BrawlersMoviePictureSelected, codec, processed + 0x200, 0x160000);
  publish(); draw();
  assert(events.size() == before_movie_faults); // Buffered old picture176 is still old.
  Event(BrawlersMovieDecodeBegin, player, frame, image_bytes, output_bytes);
  Event(BrawlersMoviePictureCopied, codec, 0xDEAD, processed + 0x200);
  Event(BrawlersMoviePictureSelected, codec, processed + 0x200, 0x160000);
  publish(); draw();
  assert(events.size() == before_movie_faults); // Unknown copy clears a reused destination.
  Event(BrawlersMovieDecodeBegin, player, frame, image_bytes, output_bytes);
  Event(BrawlersMoviePictureSelected, codec, processed, 0x160000);
  publish(); draw();
  assert(events.size() == before_movie_faults + 1); // Current picture177 can announce.
  Event(BrawlersMovieClosed, movie);
  Event(BrawlersMovieSetup, movie, 0); draw();
  const auto after_picture_checks = events.size();
  assert(after_picture_checks == before_movie_faults + 2); // Old decoder fails new lifetime floor.
  Word(codec + 0xE80, picture);
  ready_picture(177); publish();
  Event(BrawlersMovieSetup, movie, 1); draw();
  assert(events.size() == after_picture_checks); // Replacement is not verified close/reopen.
  Event(BrawlersMovieClosed, movie);
  Event(BrawlersMovieSetup, movie, 1); Event(BrawlersMovieSetup, movie, 4); draw();
  assert(events.size() == after_picture_checks); // Non-logo replacement also suppresses wording.
  Event(BrawlersMovieClosed, movie);
  Event(BrawlersMovieSetup, movie, 4); draw();
  assert(events.size() == after_picture_checks);
  Event(BrawlersMovieDecoderInvalidated, codec);
  // Actual callback ownership, final expanded text and parent/child lifetime.
  constexpr uint32_t save = 0xB000, popup = 0xC000;
  Word(0xA000, 0x82003EAC); bytes[0xA020] = 8; bytes[0xA021] = 0xCC;
  Word(save, 0x820121DC);
  Word(0xD000, 0x82012224); Word(0xD200, 0x8201221C);
  Word(0xD400, 0x82012214); Word(0xD600, 0x8201222C);
  actions[0x822D3A50] = [](PPCContext&) {
    Call(sub_822D6688, 0xD000); Call(sub_822D6238, 0xD200);
    Call(sub_822D5F48, 0xD400); Call(sub_822D6AD8, 0xD600);
  };
  const auto list_setup = [](uint32_t selector, uint32_t count) {
    PPCContext ctx{}; ctx.r3.u32 = selector; ctx.r5.u32 = count;
    sub_821DF130(ctx, opaque); assert(FlushGameSpeech());
  };
  const auto before_save = events.size();
  Call(sub_822D38D0, save);
  list_setup(save + 0x20, 1);
  Call(sub_822D3A50, save);
  Event(BrawlersNativeSelectionChanged, save + 0x20, 0);
  Event(BrawlersSaveStateChanged, save, 4);
  assert(events.size() == before_save + 1 && events.back().source == "save");
  const auto save_identity = events.back().id;
  actions[0x823A9908] = [](PPCContext&) {
    PPCContext draw{}; draw.r4.u32 = 0x4000; draw.lr = 0x823A9A24;
    sub_823A9528(draw, opaque);
  };
  const auto draw_field = [](uint32_t callback, uint32_t id, const char16_t* text) {
    Text(0x4000, text);
    actions[callback] = [id](PPCContext&) {
      PPCContext render{};
      if (id == 0) {
        render.r4.u32 = 0x4000; render.lr = 0x822D61C8;
        sub_823A9528(render, opaque);
      } else {
        render.r5.u32 = id;
        sub_823A9908(render, opaque);
      }
    };
  };
  draw_field(0x822D6798, 0xE75, u"Synthetic date 02/03/2040");
  Call(sub_822D6798, 0xDEAD); // A valid-looking callback cannot supply an unowned row.
  Call(sub_822D6798, 0xD000);
  draw_field(0x822D6368, 0xE76, u"Synthetic duration 003:04:05");
  Call(sub_822D6368, 0xD200);
  assert(events.size() == before_save + 1); // Wait for the name, including an empty name.
  draw_field(0x822D6050, 0, u""); Call(sub_822D6050, 0xD400);
  assert(events.back().label == "Synthetic date 02/03/2040");
  assert(events.back().description == "Synthetic duration 003:04:05");
  assert(events.back().position == 1 && events.back().total == 1);
  const auto cached_save_reads = reads;
  Call(sub_822D6798, 0xD000); Call(sub_822D6368, 0xD200); Call(sub_822D6050, 0xD400);
  assert(reads == cached_save_reads);

  Event(BrawlersSaveStateChanged, save, 6);
  Word(popup, 0x82003C70);
  Call(sub_821DFBE8, popup); // Synthetic bank and message identifier zero.
  Word(popup, 0x82003C80);
  list_setup(popup + 0x18, 2);
  Event(BrawlersNativeSelectionChanged, popup + 0x18, 1);
  Event(BrawlersPopupStateChanged, popup, 2);
  actions[0x821E1790] = [](PPCContext&) {
    PPCRegister record{}; record.u32 = popup + 0x74; BrawlersTextRecord(record);
    PPCContext render{}; sub_823A9908(render, opaque);
  };
  Text(0x4000, u"Synthetic question with resolved value 42");
  Call(sub_821E1790, 0xDEAD); // Native record event establishes ownership.
  assert(events[events.size() - 2].body == "Synthetic question with resolved value 42");
  assert(events.back().label_key == "popup_no" && events.back().position == 2);
  const auto popup_identity = events.back().id;
  Event(BrawlersNativeSelectionChanged, popup + 0x18, 0);
  assert(events.back().label_key == "popup_yes" && events.back().position == 1);
  Event(BrawlersPopupStateChanged, popup, 3);
  Call(sub_823B87A8, popup); Call(sub_821DF478, popup);
  assert(events.back().kind == "closed" && events.back().id == popup_identity);
  Event(BrawlersSaveStateChanged, save, 4);
  draw_field(0x822D6798, 0xE75, u"Synthetic changed date"); Call(sub_822D6798, 0xD000);
  draw_field(0x822D6368, 0xE76, u"Synthetic changed duration"); Call(sub_822D6368, 0xD200);
  draw_field(0x822D6050, 0, u"Synthetic player"); Call(sub_822D6050, 0xD400);
  assert(events.back().source == "save" && events.back().kind == "resumed");
  assert(events.back().id == save_identity && events.back().label == "Synthetic player, Synthetic changed date");
  Event(BrawlersSaveStateChanged, save, 6);
  Event(BrawlersSaveStateChanged, save, 4);
  draw_field(0x822D6BD0, 0xA3F, u"Synthetic empty slot"); Call(sub_822D6BD0, 0xD600);
  assert(events.back().label == "Synthetic empty slot" && events.back().description.empty());
  // One-choice subtype and scrolling hints use their actual final draw text.
  Word(popup, 0x82003C70);
  PPCContext single{}; single.r3.u32 = popup; single.r4.u32 = 1;
  sub_821DFBE8(single, opaque); assert(FlushGameSpeech());
  Word(popup, 0x82003C90); list_setup(popup + 0x18, 1);
  Event(BrawlersNativeSelectionChanged, popup + 0x18, 0);
  Event(BrawlersPopupStateChanged, popup, 2);
  Text(0x4000, u"Synthetic notice"); Call(sub_821E1790, 0xDEAD);
  assert(events.back().label_key == "popup_ok" && events.back().total == 1);
  actions[0x821EA650] = [](PPCContext&) {
    PPCRegister record{}; record.u32 = 0xA094; BrawlersTextRecord(record);
    PPCContext draw{}; draw.r4.u32 = 0x4000; draw.lr = 0x823AA75C;
    sub_823A9A38(draw, opaque);
  };
  Text(0x4000, u"Synthetic complete scrolling hint"); Call(sub_821EA650, 0xDEAD);
  assert(events.back().hints == std::vector<std::string>{"Synthetic complete scrolling hint"});
  const auto after_hint = events.size(), before_cached_hint_reads = static_cast<size_t>(reads);
  Call(sub_821EA650, 0xDEAD);
  assert(events.size() == after_hint && reads == before_cached_hint_reads);
  Call(sub_821DF478, popup);
  Call(sub_822D3830, save); Call(sub_822D30D8, save);
  const auto after_save = events.size();
  Call(sub_822D6BD0, 0xD600); Call(sub_823B87A8, save);
  assert(events.size() == after_save && events.back().kind == "closed");

  std::printf("semantic lifecycle fixtures passed: %u original calls, %u bounded reads\n", originals, reads);
}
