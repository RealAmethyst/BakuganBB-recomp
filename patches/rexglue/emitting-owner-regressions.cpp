#include <rex/codegen/codegen_context.h>
#include <rex/codegen/test_support.h>
#include "ppc/instruction.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace rex::codegen;
using json = nlohmann::json;
constexpr uint32_t base = 0x82000000;
uint32_t branch(unsigned from, unsigned to) {
  return 0x48000000u | ((to - from) & 0x03fffffcu);
}
uint32_t beq(unsigned from, unsigned to) {
  return 0x41820000u | ((to - from) & 0xfffcu);
}
struct Case {
  const char* name;
  uint32_t instruction;
  unsigned target;
  bool call;
  bool import;
  bool table;
  bool expected_error;
  const char* expected;
};

json run(const Case& c, const std::filesystem::path& output) {
  // Owner has blocks [0,8) and [16,32). A separate callable at 8 lies
  // between them. Its address precedes the owner's later branch at 16.
  std::vector<uint32_t> words = {beq(0, 24), branch(4, 16), 0x4e800020,
      0x60000000, c.instruction, 0x38600007, 0x38800009, 0x4e800020};
  std::vector<uint8_t> bytes;
  for (auto word : words)
    for (int shift = 24; shift >= 0; shift -= 8) bytes.push_back(uint8_t(word >> shift));
  TestModule module;
  module.Load(base, bytes.data(), bytes.size());
  auto binary = BinaryView::fromModule(module);
  RecompilerConfig config;
  config.projectName = "original_emitting_owner_regressions";
  FunctionGraph graph;
  auto* owner = graph.addFunction(base, 32, FunctionAuthority::DISCOVERED, true);
  FunctionNode* other;
  if (c.import) {
    other = graph.addImportFunction(base + 8, "SyntheticImport");
    other->discoverAsImport();
  } else {
    other = graph.addFunction(base + 8, 4, FunctionAuthority::DISCOVERED, true);
  }
  std::vector<ppc::Instruction> decoded;
  for (unsigned i = 0; i < words.size(); ++i)
    decoded.push_back(ppc::decode_instruction(base + i * 4, words[i]));
  std::vector<ppc::Instruction*> owned;
  for (auto index : {0, 1, 4, 5, 6, 7}) owned.push_back(&decoded[index]);
  owner->discover({{base, 8}, {base + 16, 16}}, owned,
                  {base, base + 16, base + 24, base + 28});
  if (!c.import) other->discover({{base + 8, 4}}, {&decoded[2]}, {});
  if (c.target == 0 || c.target == 8) {
    auto target = c.import ? CallTarget::import(base + 8, "SyntheticImport")
                          : CallTarget::function(c.target == 0 ? owner : other);
    if (c.call) graph.addCallToFunction(base, base + 16, target);
    else if (c.target != 0) graph.addTailCallToFunction(base, base + 16, target);
  }
  if (c.table) {
    JumpTable jt{base + 16, 0, 3, {base + 24, base + 28}};
    config.switchTables.emplace(base + 16, jt);
    graph.addJumpTableToFunction(base, jt);
  }
  owner->seal();
  other->seal();
  EmitContext emit{binary, config, graph};
  auto text = owner->emitCpp(emit);
  std::ofstream(output / (std::string(c.name) + ".cpp")) << text;
  bool error = text.find("REX_FATAL") != std::string::npos;
  bool expected = text.find(c.expected) != std::string::npos;
  bool has_target_label = text.find("loc_82000018:") != std::string::npos;
  bool wrong_lookup = graph.getFunctionContaining(base + 16) == nullptr;
  bool owner_has_source = owner->containsAddress(base + 16);
  bool pass = error == c.expected_error && expected && has_target_label &&
              wrong_lookup && owner_has_source;
  if (!c.expected_error && c.target == 0 && !c.call) {
    pass &= text.find("sub_82000000(ctx, base)") == std::string::npos;
  }
  if (c.call) pass &= text.find("ctx.lr = 0x82000014") != std::string::npos;
  if (c.table) pass &= text.find("goto loc_8200001C;") != std::string::npos;
  std::cout << c.name << " pass=" << pass << " fatal=" << error << '\n';
  return {{"name", c.name}, {"pass", pass}, {"fatal", error},
          {"expected_fragment_found", expected}, {"target_label_exists", has_target_label},
          {"instruction_lookup_misses_owner", wrong_lookup},
          {"explicit_owner_contains_instruction", owner_has_source}};
}

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  std::filesystem::path out(argv[1]);
  std::filesystem::create_directory(out);
  const Case cases[] = {
      {"internal_conditional", beq(16, 24), 24, false, false, false, false,
       "if (ctx.cr0.eq) goto loc_82000018;"},
      {"internal_direct_branch", branch(16, 24), 24, false, false, false, false,
       "goto loc_82000018;"},
      {"own_entry_loop", branch(16, 0), 0, false, false, false, false,
       "goto loc_82000000;"},
      {"internal_linked_pic_branch", branch(16, 24) | 1u, 24, true, false, false, false,
       "goto loc_82000018;"},
      {"own_entry_recursive_call", branch(16, 0) | 1u, 0, true, false, false, false,
       "sub_82000000(ctx, base);"},
      {"ordinary_tail_call", branch(16, 8), 8, false, false, false, false,
       "sub_82000008(ctx, base);"},
      {"ordinary_call", branch(16, 8) | 1u, 8, true, false, false, false,
       "sub_82000008(ctx, base);"},
      {"conditional_tail_call", beq(16, 8), 8, false, false, false, false,
       "sub_82000008(ctx, base);"},
      {"import_tail_call", branch(16, 8), 8, false, true, false, false,
       "__imp__SyntheticImport(ctx, base);"},
      {"conditional_import_tail", beq(16, 8), 8, false, true, false, false,
       "__imp__SyntheticImport(ctx, base);"},
      {"switch_internal_labels", 0x4e800420, 24, false, false, true, false,
       "goto loc_82000018;"},
      {"unknown_conditional_stays_error", beq(16, 40), 40, false, false, false, true,
       "Unresolved branch from 0x82000010 to 0x82000028"},
  };
  json results = json::array();
  bool pass = true;
  for (const auto& c : cases) {
    auto result = run(c, out);
    pass &= result["pass"].get<bool>();
    results.push_back(result);
  }
  std::ofstream(out / "results.json") << results.dump(2) << '\n';
  return pass ? 0 : 1;
}
