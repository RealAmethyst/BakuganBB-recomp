#!/usr/bin/env python3
"""Exercise guarded reads against actual SDK heap/mapping methods on Linux x86-64.

Run only in an isolated environment without desktop, bus, audio or game resources.
Requires a patched SDK tree, a compatible built runtime library, and Clang.
Uses UBSan: SDK fixed-address arena mappings conflict with the ASan shadow layout.
"""
import argparse
import json
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("runtime", type=Path, help="SDK tree with safe-guest-read.patch applied")
parser.add_argument("output", type=Path, help="new fixture/output directory")
parser.add_argument("--library", type=Path, required=True, help="matching librexruntime.so")
parser.add_argument("--build-include", type=Path, help="SDK generated include directory")
parser.add_argument("--compiler", default="clang++")
args = parser.parse_args()
runtime = args.runtime.resolve()
root = args.output.resolve()
library = args.library.resolve(strict=True)
root.mkdir(parents=True, exist_ok=False)
source = (runtime / "src/system/xmemory.cpp").read_text()


def method(signature):
    # Pinned method bodies contain balanced braces, including their comments.
    start = source.index(signature)
    position = source.index("{", start) + 1
    depth = 1
    while depth:
        if source[position] == "{":
            depth += 1
        elif source[position] == "}":
            depth -= 1
        position += 1
    return source[start:position] + "\n"


methods = ["bool Memory::TryReadVirtual(", "rex::memory::PageAccess ToPageAccess(",
           "uint32_t FromPageAccess(", "bool BaseHeap::Decommit(", "bool BaseHeap::Protect(",
           "bool PhysicalHeap::Decommit(", "bool PhysicalHeap::Protect("]
notice = source[:source.index("#include <algorithm>")]
headers = """#include <algorithm>
#include <climits>
#include <cstring>
#include <rex/assert.h>
#include <rex/logging.h>
#include <rex/system/xmemory.h>
namespace rex::memory {
uint32_t get_page_count(uint32_t value, uint32_t page_size, uint32_t page_size_shift);
"""
(root / "actual-methods.cpp").write_text(
    notice + headers + "\n".join(method(name) for name in methods) + "}\n")
fixture = r'''
#include <rex/system/xmemory.h>
#include <rex/logging.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cstring>
#include <iostream>
#include <limits>
#include <thread>
#include <vector>

using namespace rex::memory;
constexpr uint32_t kReadWrite = kMemoryProtectRead | kMemoryProtectWrite;
constexpr uint32_t kAllocate = kMemoryAllocationReserve | kMemoryAllocationCommit;

void CheckFailure(Memory& memory, uint32_t address, size_t length) {
  std::array<unsigned char, 32> out;
  out.fill(0xA5);
  assert(!memory.TryReadVirtual(address, out.data(), length));
  assert(std::all_of(out.begin(), out.end(), [](auto b) { return b == 0xA5; }));
}

void Stress(Memory& memory, BaseHeap* heap, uint32_t address, uint32_t size,
            bool decommit) {
  std::atomic<bool> start{false}, blocked{false};
  std::atomic<unsigned> sampled{0};
  std::atomic<unsigned> successes{0}, failures{0};
  std::vector<std::thread> readers;
  for (int thread = 0; thread < 3; ++thread) {
    readers.emplace_back([&] {
      while (!blocked.load()) std::this_thread::yield();
      CheckFailure(memory, address, 32);
      ++failures;
      ++sampled;
      while (!start.load()) std::this_thread::yield();
      for (int n = 0; n < 20000; ++n) {
        std::array<unsigned char, 32> out;
        out.fill(0xA5);
        if ((n & 63) == 0) std::this_thread::yield();
        if (memory.TryReadVirtual(address, out.data(), out.size())) {
          assert(std::all_of(out.begin(), out.end(), [](auto b) { return b == 0x63; }));
          ++successes;
        } else {
          assert(std::all_of(out.begin(), out.end(), [](auto b) { return b == 0xA5; }));
          ++failures;
        }
      }
    });
  }
  std::thread writer([&] {
    if (decommit) assert(heap->Decommit(address, size));
    else assert(heap->Protect(address, size, kMemoryProtectNoAccess));
    blocked = true;
    while (sampled.load() != 3) std::this_thread::yield();
    if (decommit) {
      assert(heap->AllocFixed(address, size, heap->page_size(), kMemoryAllocationCommit,
                             kReadWrite));
    } else assert(heap->Protect(address, size, kReadWrite));
    start = true;
    for (int n = 0; n < 4000; ++n) {
      if (decommit) {
        assert(heap->Decommit(address, size));
        std::this_thread::yield();
        assert(heap->AllocFixed(address, size, heap->page_size(), kMemoryAllocationCommit,
                               kReadWrite));
      } else {
        assert(heap->Protect(address, size, kMemoryProtectNoAccess));
        std::this_thread::yield();
        assert(heap->Protect(address, size, kReadWrite));
      }
    }
  });
  writer.join();
  for (auto& reader : readers) reader.join();
  std::array<unsigned char, 32> final{};
  assert(memory.TryReadVirtual(address, final.data(), final.size()));
  ++successes;
  assert(successes > 0);
  assert(failures > 0);
  std::cout << (decommit ? "decommit " : "protect ") << std::hex << address << std::dec
            << ": success=" << successes << " rejected=" << failures << '\n';
}

int main() {
  rex::InitLogging();
  Memory memory;
  CheckFailure(memory, 0x10000, 1);
  assert(memory.Initialize());
  CheckFailure(memory, 0, 1);
  CheckFailure(memory, 0x10000, 0);
  CheckFailure(memory, 0x10000, std::numeric_limits<size_t>::max());
  CheckFailure(memory, 0xFFFFFFFE, 4);
  CheckFailure(memory, 0x7FC80000, 4);
  CheckFailure(memory, 0x7FEA0000, 4);
  CheckFailure(memory, 0x3FFFFFF0, 32);
  assert(!memory.TryReadVirtual(0x10000, nullptr, 1));

  for (uint32_t base : {0x01000000u, 0x41000000u, 0x81000000u, 0x91000000u,
                        0xA1000000u, 0xC2000000u, 0xE3000000u}) {
    BaseHeap* heap = memory.LookupHeap(base);
    assert(heap);
    uint32_t size = heap->page_size() * 2;
    assert(heap->AllocFixed(base, size, heap->page_size(), kAllocate, kReadWrite));
    std::memset(memory.TranslateVirtual(base), 0x63, size);
    std::array<unsigned char, 32> out{};
    assert(memory.TryReadVirtual(base, out.data(), out.size()));
    assert(out.front() == 0x63 && out.back() == 0x63);
    assert(memory.TryReadVirtual(base + heap->page_size() - 16, out.data(), out.size()));
    assert(heap->Protect(base + heap->page_size(), heap->page_size(), kMemoryProtectNoAccess));
    CheckFailure(memory, base + heap->page_size() - 16, out.size());
    assert(heap->Protect(base + heap->page_size(), heap->page_size(), kReadWrite));
    if (heap->heap_type() == HeapType::kGuestPhysical) {
      memory.EnablePhysicalMemoryAccessCallbacks(memory.GetPhysicalAddress(base), 32, true, false);
      CheckFailure(memory, base, out.size());
      assert(heap->Protect(base, size, kReadWrite));
      assert(memory.TryReadVirtual(base, out.data(), out.size()));
      auto* parent = memory.GetPhysicalHeap();
      uint32_t physical = memory.GetPhysicalAddress(base);
      assert(parent->Protect(physical, size, kMemoryProtectNoAccess));
      CheckFailure(memory, base, out.size());
      assert(parent->Protect(physical, size, kReadWrite));
    }
    if (base == 0x01000000u || base == 0xA1000000u || base == 0xE3000000u) {
      Stress(memory, heap, base, size, false);
      Stress(memory, heap, base, size, true);
    }
    assert(heap->Release(base));
    CheckFailure(memory, base, out.size());
  }
  std::cout << "guarded read tests passed\n";
}
'''
(root / "synthetic-safe-read.cpp").write_text(fixture)
flags = ["-std=c++23", "-march=x86-64-v2", "-DREX_PLATFORM_LINUX=1",
         "-DSPDLOG_COMPILED_LIB", "-DSPDLOG_FMT_EXTERNAL", "-Wall", "-Wextra",
         "-Wno-unused-parameter", "-Werror", "-pthread", "-g", "-O1",
         "-fsanitize=undefined", "-fno-sanitize-recover=all"]
includes = [runtime / "include", runtime / "thirdparty", runtime / "thirdparty/simde",
            runtime / "thirdparty/fmt/include", runtime / "thirdparty/spdlog/include",
            runtime / "thirdparty/sdl3/include", runtime / "thirdparty/tomlplusplus/include"]
if args.build_include:
    includes.insert(0, args.build_include.resolve())
for include in includes:
    flags.extend(["-I", str(include)])
commands = [
    [args.compiler, *flags, "-fsyntax-only", str(runtime / "src/system/xmemory.cpp")],
    [args.compiler, *flags, str(root / "actual-methods.cpp"),
     str(root / "synthetic-safe-read.cpp"), str(library),
     "-Wl,-rpath," + str(library.parent), "-Wl,--export-dynamic",
     "-o", str(root / "synthetic-safe-read")],
    [str(root / "synthetic-safe-read")]]
results = []
for index, command in enumerate(commands):
    result = subprocess.run(command, text=True, capture_output=True)
    (root / f"{index}.stdout").write_text(result.stdout)
    (root / f"{index}.stderr").write_text(result.stderr)
    results.append({"command": command, "returncode": result.returncode})
    (root / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    print(result.stdout, end="")
    print(result.stderr, end="")
    if result.returncode:
        raise SystemExit(result.returncode)
