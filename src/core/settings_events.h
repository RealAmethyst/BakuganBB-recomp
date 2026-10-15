#pragma once

#include "native_speech.h"
#include "settings_menu.h"

namespace brawlers::settings {

// Produces copied events for the shared game FIFO. The owner supplies a unique
// screen identity and calls Closed only after actual native screen closure.
class Events {
 public:
  std::vector<ScreenEvent> Present(uint64_t identity, uint32_t locale,
                                   Event event, const View& view);
  std::vector<ScreenEvent> Closed();

 private:
  ScreenEvent Make(const char* source, const char* kind) const;
  ScreenEvent Item(const char* kind, const View& view) const;
  uint64_t identity_ = 0;
  uint32_t locale_ = 0;
  bool confirming_ = false;
};

}  // namespace brawlers::settings
