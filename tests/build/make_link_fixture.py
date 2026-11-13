#!/usr/bin/env python3
"""Create original, non-runnable translation stand-ins for native host linking.

Only public project hook source is read. No game input or generated translation
is used. The resulting host must never be run, installed or distributed.
"""

import argparse
from pathlib import Path
import re


PROJECT = Path(__file__).resolve().parents[2]
NOTICE = "// Original synthetic link fixture. Never run or distribute the host.\n"


def hook_names():
    names = set()
    for directory in (PROJECT / "src/game", PROJECT / "src/runtime"):
        for source in sorted(directory.iterdir()):
            if source.suffix not in (".cpp", ".h"):
                continue
            text = source.read_text(encoding="utf-8")
            text = re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.DOTALL)
            names.update(re.findall(r"\b(?:__imp__)?(sub_[0-9A-Fa-f]{8})\b", text))
            names.update("sub_" + address for address in
                         re.findall(r"\bORIGINAL\(\s*([0-9A-Fa-f]{8})\s*\)", text))
    if not names:
        raise ValueError("No public native hook symbols were found")
    return sorted(names)


def write_fixture(output):
    output = output.resolve()
    if output == PROJECT or PROJECT in output.parents:
        raise ValueError("The fixture output must be outside the repository")
    names = hook_names()
    output.mkdir(parents=True, exist_ok=False)

    pch = NOTICE + """#pragma once
#define BRAWLERS_SYNTHETIC_LINK_FIXTURE 1
#include <cstdlib>
#include <rex/ppc.h>
#include <rex/image_info.h>

extern const rex::PPCImageInfo PPCImageConfig;
extern PPCFuncMapping PPCFuncMappings[];
"""
    init = NOTICE + '#pragma once\n#include "battle_brawlers_pch.h"\n'
    body = NOTICE + """#include "battle_brawlers_init.h"

PPCFuncMapping PPCFuncMappings[] = {{0, nullptr}};
const rex::PPCImageInfo PPCImageConfig = {
    .code_base = 0, .code_size = 0, .image_base = 0, .image_size = 0,
    .func_mappings = PPCFuncMappings,
};

// Exercise Windows SEH compilation with the translation target's PCH/options.
// This function and all guest stand-ins below must remain unexecuted.
#if defined(_WIN32)
extern "C" void brawlers_fixture_seh(volatile unsigned* value) {
  __try {
    *value = 0;
  } __except (1) {
    std::abort();
  }
}
#endif

// Independently written declarations follow ReXGlue's public weak-alias ABI:
// rexglue-sdk c94f5ebdcb3c9d1a460ca48e04f9758448f8d518,
// resources/templates/codegen/pch_h.inja (DEFINE_REX_FUNC).
// Strong project hooks replace the alias; __imp__ always names the stand-in.
"""
    for name in names:
        body += f"""
extern "C" void __imp__{name}(PPCContext&, std::uint8_t*) {{
  std::abort();
}}
extern "C" void {name}(PPCContext&, std::uint8_t*)
    __attribute__((weak, noinline, alias("__imp__{name}")));
"""
    sources = """# Original synthetic link fixture; no game inputs are used.
set(GENERATED_SOURCES "${CMAKE_CURRENT_LIST_DIR}/battle_brawlers_fixture.cpp")
"""
    for name, content in (
            ("battle_brawlers_pch.h", pch), ("battle_brawlers_init.h", init),
            ("battle_brawlers_fixture.cpp", body), ("sources.cmake", sources)):
        with (output / name).open("x", encoding="utf-8", newline="\n") as stream:
            stream.write(content)
    return len(names)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path,
                        help="New external directory for the synthetic link fixture")
    arguments = parser.parse_args()
    try:
        count = write_fixture(arguments.output)
    except (OSError, ValueError) as error:
        parser.exit(1, f"Fixture creation failed: {error}\n")
    print(f"Created synthetic link fixture with {count} aborting guest stand-ins.")
    print("Compile and link only; never run, install or distribute the host.")


if __name__ == "__main__":
    main()
