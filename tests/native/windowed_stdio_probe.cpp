// Original fixture: verify redirected streams for the native host's subsystem.
// This does not link the game, SDL, ReXGlue or a screen-reader library.
#include <windows.h>
#include <cstdio>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  std::fprintf(stderr, "BRAWLERS_STATUS:thread_created\n");
  std::fflush(stderr);
  std::puts("synthetic stdout");
  return 0;
}
