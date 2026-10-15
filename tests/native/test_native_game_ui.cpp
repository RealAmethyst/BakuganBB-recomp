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
HOOK(8232C3E0);
HOOK(8232BB28);
HOOK(8232BC00);
HOOK(8232EB88);
HOOK(8232E448);
HOOK(8232E830);
HOOK(82349F58); HOOK(82348548); HOOK(8234A890); HOOK(8234C0D0);
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
void BrawlersCreatorStateChanged(PPCRegister&, PPCRegister&);
void BrawlersCreatorTransitionChanged(PPCRegister&, PPCRegister&);
void BrawlersCreatorMarkerAbsent(PPCRegister&);
void BrawlersAttributeStateChanged(PPCRegister&, PPCRegister&);

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
STUB(8232C3E0)
STUB(8232BB28)
STUB(8232BC00)
STUB(8232EB88)
STUB(8232E448)
STUB(8232E830)
STUB(82349F58); STUB(82348548); STUB(8234A890); STUB(8234C0D0);
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

  // No-choice operation messages use the same owned final text, without a list.
  const auto before_status = events.size();
  Word(popup, 0x82003C70);
  PPCContext status{};
  status.r3.u32 = popup; status.r4.u32 = 2;
  status.r6.u32 = 4; status.r7.u32 = 99;
  sub_821DFBE8(status, opaque); assert(FlushGameSpeech());
  Word(popup, 0x82003CB0);
  Event(BrawlersPopupStateChanged, popup, 2);
  assert(events.size() == before_status); // A request is not displayed text.
  Text(0x4000, u"Synthetic operation in progress");
  const auto before_wrong_body = reads;
  Call(sub_821E1790, 0xDEAD); // The prior synthetic bank/ID do not match.
  assert(events.size() == before_status && reads == before_wrong_body);
  actions[0x821E1790] = [](PPCContext&) {
    PPCRegister record{}; record.u32 = popup + 0x74; BrawlersTextRecord(record);
    PPCContext render{}; render.r4.u32 = 4; render.r5.u32 = 99;
    sub_823A9908(render, opaque);
  };
  Call(sub_821E1790, 0xDEAD);
  assert(events.size() == before_status + 1);
  assert(events.back().source == "status" && events.back().kind == "opened");
  assert(events.back().body == "Synthetic operation in progress");
  assert(events.back().position == 0 && events.back().total == 0);
  assert(events.back().label.empty() && events.back().label_key.empty());
  assert(events.back().hints.empty());
  const auto status_identity = events.back().id;
  const auto cached_status_reads = reads;
  Call(sub_821E1790, 0xDEAD); Call(sub_821EA650, 0xDEAD);
  assert(events.size() == before_status + 1 && reads == cached_status_reads);
  Event(BrawlersPopupStateChanged, popup, 3);
  Call(sub_823B87A8, popup); Call(sub_821DF478, popup);
  assert(events.size() == before_status + 2 && events.back().source == "status");
  assert(events.back().kind == "closed" && events.back().id == status_identity);
  Call(sub_821E1790, 0xDEAD);
  assert(events.size() == before_status + 2);
  // A removed request with no text rendering never manufactures a message.
  Word(popup, 0x82003C70);
  status = {}; status.r3.u32 = popup; status.r4.u32 = 2;
  sub_821DFBE8(status, opaque); assert(FlushGameSpeech());
  Word(popup, 0x82003CB0);
  Event(BrawlersPopupStateChanged, popup, 2);
  Call(sub_821DF478, popup);
  assert(events.size() == before_status + 2);
  Call(sub_822D3830, save); Call(sub_822D30D8, save);
  const auto after_save = events.size();
  Call(sub_822D6BD0, 0xD600); Call(sub_823B87A8, save);
  assert(events.size() == after_save && events.back().kind == "closed");

  // Original synthetic creator state: seven enabled category rows plus reserved row eight.
  constexpr uint32_t creator = 0xE000, enabled = 0xF000;
  Word(creator, 0x820196BC); Word(creator + 0x2C, 1); Word(creator + 0x9C, UINT32_MAX);
  Word(creator + 0xB8, 0x8200FC30); Word(creator + 0xE0, creator + 0xF8);
  Word(creator + 0xF8, 0x8200FC10); Word(creator + 0x100, enabled);
  Word(creator + 0x108, 0); Word(creator + 0x10C, 9);
  bytes[0x82019469] = 7; Word(0x829A2008, 0x82019460);
  bytes[0x82019460] = 1; bytes[0x82019461] = 2;
  bytes[0x820194FE] = 0; bytes[0x820194FF] = 100;
  bytes[0x82019500] = 0; bytes[0x82019501] = 101;
  for (uint32_t row = 0; row < 9; ++row) bytes[enabled + row] = row != 7;
  Call(sub_8232C3E0, creator); Call(sub_8232BC00, creator);
  const auto creator_before = events.size();
  Event(BrawlersCreatorTransitionChanged, creator, 1);
  assert(events.size() == creator_before);
  Event(BrawlersCreatorStateChanged, creator, 3);
  assert(events.size() == creator_before + 1 && events.back().source == "creator");
  assert(events.back().kind == "opened" && events.back().title.empty());
  const auto creator_identity = events.back().id;
  const auto render_creator = [&](uint32_t address, uint32_t id, const char16_t* value,
                                  uint32_t record = creator + 0x68) {
    Text(0xFC00, value);
    actions[0x823A9908] = [](PPCContext&) {
      PPCContext final{}; final.r4.u32 = 0xFC00; final.lr = 0x823A9A24;
      sub_823A9528(final, opaque);
    };
    actions[address] = [=](PPCContext&) {
      PPCRegister native_record{}; native_record.u32 = record; BrawlersTextRecord(native_record);
      if (!id && address == 0x8232E830) {
        PPCRegister widget{}; BrawlersCreatorMarkerAbsent(widget);
      }
      if (id) {
        PPCContext render{}; render.r4.u32 = 0; render.r5.u32 = id;
        sub_823A9908(render, opaque);
      }
    };
    if (address == 0x8232EB88) Call(sub_8232EB88, 0xDEAD);
    else if (address == 0x8232E830) Call(sub_8232E830, 0xDEAD);
    else if (address == 0x8232E448) Call(sub_8232E448, 0xDEAD);
    else Call(sub_821EA650, 0xDEAD);
  };
  render_creator(0x8232EB88, 100, u"Synthetic category");
  assert(events.size() == creator_before + 1); // Marker decision has not rendered yet.
  render_creator(0x8232E830, 0, u"");
  assert(events.back().label == "Synthetic category" && events.back().position == 1);
  assert(events.back().total == 8 && events.back().section.empty());
  Text(0xFC00, u"Synthetic select and back");
  actions[0x821EA650] = [](PPCContext&) {
    PPCRegister record{}; record.u32 = 0xA094; BrawlersTextRecord(record);
    PPCContext draw{}; draw.r4.u32 = 0xFC00; draw.lr = 0x821EACD8;
    sub_823A9528(draw, opaque); // Composite hint path has no localization wrapper.
  };
  Call(sub_821EA650, 0xDEAD);
  render_creator(0x8232E448, 0xA23, u"Synthetic rotate");
  assert(events.back().hints.size() == 2 && events.back().id == creator_identity);
  auto creator_cached_reads = reads;
  auto creator_cached_events = events.size();
  for (unsigned repeat = 0; repeat < 5; ++repeat) {
    render_creator(0x8232EB88, 100, u"Already captured");
    render_creator(0x8232E830, 0, u"");
    render_creator(0x8232E448, 0xA23, u"Already captured");
    render_creator(0x821EA650, 0x9EC, u"Already captured", 0xA094);
  }
  assert(reads == creator_cached_reads && events.size() == creator_cached_events);
  Event(BrawlersNativeSelectionChanged, creator + 0xB8, 1);
  creator_cached_reads = reads;
  render_creator(0x8232EB88, 100, u"Outgoing crossfade");
  assert(reads == creator_cached_reads && events.size() == creator_cached_events);
  render_creator(0x8232E830, 0, u"");
  render_creator(0x8232EB88, 101, u"Synthetic second category");
  assert(events.back().label == "Synthetic second category" && events.back().position == 2);
  assert(events.back().hints.empty());
  creator_cached_events = events.size();
  Event(BrawlersNativeSelectionChanged, creator + 0xB8, 7);
  render_creator(0x8232EB88, 101, u"Disabled hole");
  assert(events.size() == creator_cached_events);
  Event(BrawlersNativeSelectionChanged, creator + 0xB8, 8);
  assert(events.back().label_key == "popup_ok" && events.back().position == 8);
  assert(events.back().total == 8 && events.back().description_key.empty());
  creator_cached_reads = reads;
  render_creator(0x8232EB88, 101, u"Last category is not the reserved button");
  assert(reads == creator_cached_reads);

  // A native palette configuration is a subscreen transition, retaining the screen identity.
  Word(creator + 0x9C, 3); bytes[0x820194B7] = 3; Word(creator + 0x108, 1);
  for (uint32_t row = 0; row < 9; ++row) bytes[enabled + row] = row < 3;
  bytes[0x82019562] = 1; bytes[0x82019563] = 44; // Synthetic ID300, category3/row1.
  bytes[0x82019564] = 1; bytes[0x82019565] = 45;
  Call(sub_8232BC00, creator);
  creator_cached_events = events.size();
  render_creator(0x8232EB88, 300, u"Not active yet");
  assert(events.size() == creator_cached_events);
  Event(BrawlersCreatorTransitionChanged, creator, 1);
  render_creator(0x8232EB88, 300, u"Synthetic color");
  // A marker animation gate is not evidence that the conditional marker is absent.
  actions[0x8232E830] = [](PPCContext&) {
    PPCRegister record{}; record.u32 = creator + 0x68; BrawlersTextRecord(record);
  };
  Call(sub_8232E830, 0xDEAD);
  assert(events.size() == creator_cached_events);
  render_creator(0x8232E830, 0x927, u"Synthetic accepted marker");
  assert(events.back().kind == "resumed" && events.back().id == creator_identity);
  assert(events.back().label == "Synthetic color" && events.back().selected);
  assert(events.back().description.empty());
  assert(events.back().position == 2 && events.back().total == 3);
  assert(events.back().description_key == "creator_3_1" && events.back().section.empty());
  Event(BrawlersNativeSelectionChanged, creator + 0xB8, 2);
  render_creator(0x8232E830, 0, u"");
  render_creator(0x8232EB88, 301, u"Synthetic next color");
  assert(events.back().kind == "focused" && events.back().description.empty());
  assert(!events.back().selected);
  assert(events.back().position == 3 && events.back().total == 3);
  // Back stores the closing transition before the state-three event; no stale resume.
  creator_cached_events = events.size();
  Event(BrawlersCreatorStateChanged, creator, 5);
  Event(BrawlersCreatorTransitionChanged, creator, 3);
  Event(BrawlersCreatorStateChanged, creator, 3);
  assert(events.size() == creator_cached_events);
  Word(creator + 0x9C, UINT32_MAX); Word(creator + 0x108, 1);
  for (uint32_t row = 0; row < 9; ++row) bytes[enabled + row] = row != 7;
  Call(sub_8232BC00, creator);
  Event(BrawlersCreatorTransitionChanged, creator, 1);
  render_creator(0x8232E830, 0, u"");
  render_creator(0x8232EB88, 101, u"Synthetic second category");
  assert(events.back().kind == "resumed" && events.back().id == creator_identity);
  assert(events.back().position == 2 && events.back().total == 8);
  // A child popup suspends, then resumes the same creator without repeating hints.
  Event(BrawlersCreatorStateChanged, creator, 6);
  creator_cached_events = events.size();
  render_creator(0x8232EB88, 301, u"Hidden parent");
  assert(events.size() == creator_cached_events);
  Event(BrawlersCreatorStateChanged, creator, 3);
  assert(events.back().kind == "resumed" && events.back().id == creator_identity);
  assert(events.back().hints.empty());

  creator_cached_events = events.size();
  // Preview-loading text still reaches the game's renderer without becoming speech.
  // No creator wrapper or text-record hook participates in this native path.
  const auto preview_reads = reads;
  Text(0xFC00, u"Synthetic actor loading");
  PPCContext preview{}; preview.r4.u32 = 0; preview.r5.u32 = 0x91F;
  sub_823A9908(preview, opaque);
  assert(events.size() == creator_cached_events && reads == preview_reads);
  // Malformed reconfiguration invalidates the old snapshot and cannot speak stale text.
  bytes[enabled + 7] = 1;
  Call(sub_8232BC00, creator);
  Event(BrawlersCreatorTransitionChanged, creator, 1);
  render_creator(0x8232EB88, 301, u"Invalid configured list");
  assert(events.size() == creator_cached_events);
  Call(sub_8232BB28, creator); Call(sub_823B87A8, creator);
  assert(events.back().source == "creator" && events.back().kind == "closed");
  assert(events.back().id == creator_identity);

  // Story mode has eight categories and all three actual hints, including randomize.
  Word(creator + 0x2C, 0); Word(creator + 0x9C, UINT32_MAX); Word(creator + 0x108, 0);
  bytes[0x82019468] = 8; Word(0x829A2004, 0x82019458); bytes[0x82019458] = 1;
  for (uint32_t row = 0; row < 9; ++row) bytes[enabled + row] = 1;
  Call(sub_8232C3E0, creator); Call(sub_8232BC00, creator);
  Event(BrawlersCreatorTransitionChanged, creator, 1);
  Event(BrawlersCreatorStateChanged, creator, 3);
  render_creator(0x8232E830, 0, u"");
  render_creator(0x8232EB88, 100, u"Synthetic story category");
  assert(events.back().total == 9 && events.back().id != creator_identity);
  render_creator(0x821EA650, 0x9EC, u"Synthetic select and back", 0xA094);
  render_creator(0x8232E448, 0xA23, u"Synthetic rotate");
  assert(events.back().hints.empty());
  render_creator(0x8232E448, 0x94A, u"Synthetic randomize");
  assert(events.back().hints.size() == 3);
  Event(BrawlersNativeSelectionChanged, creator + 0xB8, 8);
  assert(events.back().position == 9 && events.back().total == 9);
  Call(sub_823B87A8, creator); Call(sub_8232BB28, creator);

  // Leaving the root before its Random hint renders must not prevent submenu hints.
  Word(creator + 0x108, 0);
  Call(sub_8232C3E0, creator); Call(sub_8232BC00, creator);
  Event(BrawlersCreatorTransitionChanged, creator, 1);
  Event(BrawlersCreatorStateChanged, creator, 3);
  render_creator(0x8232E830, 0, u"");
  render_creator(0x8232EB88, 100, u"Synthetic story category");
  render_creator(0x821EA650, 0x9EC, u"Synthetic select and back", 0xA094);
  render_creator(0x8232E448, 0xA23, u"Synthetic rotate");
  assert(events.back().hints.empty());
  Word(creator + 0x9C, 3); Word(creator + 0x108, 1);
  for (uint32_t row = 0; row < 9; ++row) bytes[enabled + row] = row < 3;
  Call(sub_8232BC00, creator);
  Event(BrawlersCreatorTransitionChanged, creator, 1);
  render_creator(0x8232E830, 0, u"");
  render_creator(0x8232EB88, 300, u"Synthetic submenu option");
  assert(events.back().hints.size() == 2);
  Call(sub_823B87A8, creator);

  // Synthetic provider entries exercise cursor mapping independently of committed preview.
  constexpr uint32_t provider = 0x20000, bank = 0x21000, resource = 0x22000;
  Word(0x829CEB78, provider); Word(provider, 0x82036508);
  Word(provider + 8, bank); bytes[provider + 0xC] = 1; bytes[bank + 0x60] = 1;
  Word(bank, resource); Word(bank + 4, resource + 0x3000); Word(bank + 8, resource + 0x20);
  Word(resource, 0x70617374); Word(resource + 4, 0x7274626C);
  bytes[resource + 0xB] = 1; Word(resource + 0xC, 0x989);
  Word(resource + 0x10, 0x20); Word(resource + 0x14, 0x3000);
  for (uint32_t id = 0; id < 0x989; ++id) {
    Word(resource + 0x20 + id * 4, 0x5000 + id * 0x80);
    Text(resource + 0x5000 + id * 0x80, u"Synthetic text");
  }
  constexpr std::array<uint32_t, 6> attribute_names{0x656, 0x65A, 0x658, 0x659, 0x657, 0x65B};
  constexpr std::array<uint32_t, 6> attribute_descriptions{0x985, 0x983, 0x986, 0x982, 0x987, 0x984};
  for (uint32_t row = 0; row < 6; ++row) {
    const std::u16string label = u"Synthetic row " + std::u16string(1, u'1' + row);
    const std::u16string description = u"Synthetic description " + std::u16string(1, u'1' + row);
    Text(resource + 0x5000 + attribute_names[row] * 0x80, label.c_str());
    Text(resource + 0x5000 + attribute_descriptions[row] * 0x80, description.c_str());
  }
  constexpr uint32_t attribute = 0xB0000, attribute_enabled = 0xB4000;
  Word(attribute, 0x8201A8D0); Word(attribute + 0x48, 0x8200FC30);
  Word(attribute + 0x70, attribute + 0x88); Word(attribute + 0x88, 0x8200FC10);
  Word(attribute + 0x90, attribute_enabled); Word(attribute + 0x98, 4);
  Word(attribute + 0x9C, 7);
  for (uint32_t row = 0; row < 7; ++row) bytes[attribute_enabled + row] = 1;
  const auto render_attribute = [&](uint32_t address, uint32_t id, const char16_t* value,
                                    uint32_t record) {
    Text(0xFC00, value);
    actions[0x823A9908] = [](PPCContext&) {
      PPCContext final{}; final.r4.u32 = 0xFC00; final.lr = 0x823A9A24;
      sub_823A9528(final, opaque);
    };
    actions[address] = [=](PPCContext&) {
      PPCRegister native_record{}; native_record.u32 = record; BrawlersTextRecord(native_record);
      PPCContext draw{}; draw.r4.u32 = 0; draw.r5.u32 = id; sub_823A9908(draw, opaque);
    };
    if (address == 0x8234A890) Call(sub_8234A890, 0xDEAD);
    else if (address == 0x8234C0D0) Call(sub_8234C0D0, 0xDEAD);
    else Call(sub_821EA650, 0xDEAD);
  };
  Call(sub_82349F58, attribute);
  auto attribute_events = events.size();
  render_attribute(0x8234A890, 0x6B9, u"Hidden title", attribute + 0xA8);
  assert(events.size() == attribute_events);
  Event(BrawlersAttributeStateChanged, attribute, 7);
  render_attribute(0x8234A890, 0x6B9, u"Synthetic attribute title", attribute + 0xA8);
  assert(events.size() == attribute_events);
  render_attribute(0x8234C0D0, 0x96A, u"", attribute + 0xD0);
  assert(events.size() == attribute_events);
  render_attribute(0x8234C0D0, 0x96A, u"Synthetic instruction", attribute + 0xD0);
  assert(events.size() == attribute_events + 2);
  const auto attribute_identity = events.back().id;
  assert(events[attribute_events].title == "Synthetic attribute title");
  assert(events[attribute_events].body == "Synthetic instruction");
  assert(events.back().label == "Synthetic row 5" && events.back().description == "Synthetic description 5");
  assert(events.back().position == 5 && events.back().total == 7);
  render_attribute(0x821EA650, 0x9EC, u"Synthetic parent hints", 0xA094);
  assert(events.back().hints.size() == 1);
  auto attribute_reads = reads; attribute_events = events.size();
  render_attribute(0x8234A890, 0x6B9, u"Cached title", attribute + 0xA8);
  render_attribute(0x8234C0D0, 0x96A, u"Cached instruction", attribute + 0xD0);
  render_attribute(0x821EA650, 0x9EC, u"Cached hints", 0xA094);
  Event(BrawlersAttributeStateChanged, attribute, 7);
  Event(BrawlersNativeSelectionChanged, attribute + 0x48, 4);
  assert(reads == attribute_reads && events.size() == attribute_events);
  for (uint32_t row = 0; row < 6; ++row) {
    Event(BrawlersNativeSelectionChanged, attribute + 0x48, row);
    assert(events.back().label == "Synthetic row " + std::to_string(row + 1));
    assert(events.back().description == "Synthetic description " + std::to_string(row + 1));
    assert(events.back().position == row + 1 && events.back().total == 7);
    assert(events.back().hints.empty());
  }
  Event(BrawlersNativeSelectionChanged, attribute + 0x48, 6);
  assert(events.back().label_key == "popup_ok" && events.back().position == 7);
  assert(events.back().description.empty());
  Event(BrawlersAttributeStateChanged, attribute, 8);
  attribute_events = events.size(); attribute_reads = reads;
  Event(BrawlersNativeSelectionChanged, attribute + 0x48, 1);
  render_attribute(0x8234C0D0, 0x96A, u"Hidden instruction", attribute + 0xD0);
  assert(events.size() == attribute_events && reads == attribute_reads);
  Word(attribute + 0x98, 2);
  Event(BrawlersAttributeStateChanged, attribute, 7);
  assert(events.back().kind == "resumed" && events.back().id == attribute_identity);
  assert(events.back().label == "Synthetic row 3");
  assert(events.back().hints.empty());
  // A malformed provider read invalidates the new item, retaining no old caption.
  Word(resource, 0); attribute_events = events.size();
  Event(BrawlersNativeSelectionChanged, attribute + 0x48, 0);
  assert(events.size() == attribute_events);
  Word(resource, 0x70617374);
  Event(BrawlersNativeSelectionChanged, attribute + 0x48, 0);
  assert(events.back().label == "Synthetic row 1");
  Call(sub_82348548, attribute); Call(sub_823B87A8, attribute);
  assert(events.back().kind == "closed" && events.back().id == attribute_identity);
  static uint32_t removed_owner = 0;
  SetGameTaskRemovedObserver([](uint32_t owner) noexcept { removed_owner = owner; });
  Call(sub_823B87A8, 0xF000);
  assert(removed_owner == 0xF000);
  SetGameTaskRemovedObserver(nullptr);
  Call(sub_823B87A8, 0xF100);
  assert(removed_owner == 0xF000);
  std::printf("semantic lifecycle fixtures passed: %u original calls, %u bounded reads\n", originals, reads);
}
