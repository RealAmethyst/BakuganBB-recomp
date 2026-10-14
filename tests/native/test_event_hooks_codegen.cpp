// Original synthetic PPC fixture; no game instructions or assets.
#include <rex/codegen/codegen_context.h>
#include <rex/codegen/test_support.h>
#include "ppc/instruction.h"

#include <filesystem>
#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
  using namespace rex::codegen;
  if (argc != 2) return 2;
  const std::filesystem::path output(argv[1]);
  constexpr uint32_t address = 0x82000000;
  // Compute into a nonvolatile register, store it, copy it to an argument, return.
  const uint32_t words[] = {0x3bc30001, 0x93c40000, 0x7fc5f378, 0x4e800020};
  std::vector<uint8_t> bytes;
  for (auto word : words)
    for (int shift = 24; shift >= 0; shift -= 8)
      bytes.push_back(uint8_t(word >> shift));
  TestModule module;
  module.Load(address, bytes.data(), bytes.size());
  auto binary = BinaryView::fromModule(module);
  FunctionGraph graph;
  auto* function = graph.addFunction(address, 16, FunctionAuthority::DISCOVERED, true);
  std::vector<ppc::Instruction> decoded;
  for (unsigned i = 0; i < 4; ++i)
    decoded.push_back(ppc::decode_instruction(address + 4 * i, words[i]));
  std::vector<ppc::Instruction*> instructions;
  for (auto& instruction : decoded) instructions.push_back(&instruction);
  function->discover({{address, 16}}, instructions, {});
  function->seal();
  for (bool local : {false, true}) for (bool after : {false, true}) {
    RecompilerConfig config;
    config.nonVolatileRegistersAsLocalVariables = local;
    MidAsmHook hook;
    hook.name = "SyntheticStoreEvent";
    hook.registers = {"r30", "r4"};
    hook.afterInstruction = after;
    config.midAsmHooks.emplace(address + 4, hook);
    EmitContext context{binary, config, graph};
    const auto source = function->emitCpp(context);
    const auto filename = std::string(local ? "local_" : "context_") +
                          (after ? "after.cpp" : "before.cpp");
    std::ofstream file(output / filename);
    file << source;
    if (!file) return 1;
    const auto call = source.find(local ? "SyntheticStoreEvent(r30, ctx.r4);" :
                                         "SyntheticStoreEvent(ctx.r30, ctx.r4);");
    const auto store = source.find("REX_STORE_U32");
    if (call == std::string::npos || store == std::string::npos ||
        (after ? call <= store : call >= store) ||
        source.find("REX_FATAL") != std::string::npos)
      return 1;
  }
  std::cout << "Synthetic before/after store hooks emitted with context and local registers\n";
}
