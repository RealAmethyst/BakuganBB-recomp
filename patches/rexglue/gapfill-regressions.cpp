#include <rex/codegen/codegen_context.h>
#include <rex/codegen/phases.h>
#include <rex/codegen/test_support.h>

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <vector>

#include "ppc/instruction.h"

using namespace rex::codegen;
using json = nlohmann::json;
constexpr uint32_t base = 0x82000000;
constexpr uint32_t ret = 0x4e800020, li3 = 0x38600007, li4 = 0x38800009;
uint32_t b(unsigned from, unsigned to) {
  return 0x48000000u | ((to - from) & 0x03fffffcu);
}
uint32_t beq(unsigned from, unsigned to) {
  return 0x41820000u | ((to - from) & 0xfffcu);
}
struct Seed {
  unsigned offset, size;
};
struct Ownership {
  unsigned instruction, owner;
};
struct Switch {
  unsigned site;
  std::vector<unsigned> targets;
};
struct Case {
  const char* name;
  std::vector<uint32_t> words;
  std::vector<Seed> seeds;
  std::vector<Ownership> owned;
  std::vector<unsigned> no_entries;
  std::vector<unsigned> unowned;
  std::vector<Switch> switches = {};
};
bool owns(const FunctionNode& node, unsigned address) {
  bool in_block = false, in_instruction = false;
  for (const auto& block : node.blocks())
    if (block.contains(base + address)) in_block = true;
  for (const auto* insn : node.instructions())
    if (insn->address == base + address) in_instruction = true;
  return in_block && in_instruction;
}
json run(const Case& c) {
  std::vector<uint8_t> bytes;
  for (auto word : c.words)
    for (int shift = 24; shift >= 0; shift -= 8)
      bytes.push_back(uint8_t(word >> shift));
  TestModule module;
  module.Load(base, bytes.data(), bytes.size());
  RecompilerConfig config;
  config.projectName = "original_gap_regressions";
  for (const auto& table : c.switches) {
    JumpTable jt{base + table.site, 0, 3, {}};
    for (auto target : table.targets) jt.targets.push_back(base + target);
    config.switchTables.emplace(base + table.site, std::move(jt));
  }
  auto ctx =
      CodegenContext::Create(BinaryView::fromModule(module), std::move(config));
  ctx.initDecoded();
  for (auto seed : c.seeds)
    ctx.graph.addFunction(base + seed.offset, seed.size,
                          FunctionAuthority::CONFIG, true);
  bool phases_ok = bool(phases::Scan(ctx, nullptr)) &&
                   bool(phases::Discover(ctx, nullptr)) &&
                   bool(phases::GapFill(ctx, nullptr)) &&
                   bool(phases::Merge(ctx, nullptr));
  bool valid = bool(phases::Validate(ctx, nullptr));
  json j{{"name", c.name},          {"phases_ok", phases_ok},
         {"valid", valid},          {"errors", ctx.errors.Count()},
         {"checks", json::array()}, {"nodes", json::array()}};
  bool checks_ok = true;
  for (auto expectation : c.owned) {
    auto* node = ctx.graph.getFunction(base + expectation.owner);
    bool ok = node && owns(*node, expectation.instruction);
    checks_ok = checks_ok && ok;
    j["checks"].push_back({{"kind", "actual_block_and_instruction_owner"},
                           {"instruction", expectation.instruction},
                           {"owner", expectation.owner},
                           {"pass", ok}});
  }
  for (auto offset : c.no_entries) {
    bool ok = ctx.graph.getFunction(base + offset) == nullptr;
    checks_ok = checks_ok && ok;
    j["checks"].push_back(
        {{"kind", "no_surviving_entry"}, {"offset", offset}, {"pass", ok}});
  }
  for (auto offset : c.unowned) {
    bool ok = true;
    for (const auto& [entry, node] : ctx.graph.functions())
      if (owns(*node, offset)) ok = false;
    checks_ok = checks_ok && ok;
    j["checks"].push_back({{"kind", "unreachable_instruction_not_owned"},
                           {"offset", offset},
                           {"pass", ok}});
  }
  for (const auto& [entry, node] : ctx.graph.functions()) {
    json n{{"entry", entry - base},
           {"authority", AuthorityName(node->authority())},
           {"state", static_cast<int>(node->state())},
           {"blocks", json::array()},
           {"instructions", json::array()}};
    n["jump_table_count"] = node->jumpTables().size();
    for (const auto& block : node->blocks())
      n["blocks"].push_back({block.base - base, block.size});
    for (const auto* insn : node->instructions())
      n["instructions"].push_back(insn->address - base);
    j["nodes"].push_back(std::move(n));
  }
  j["pass"] = phases_ok && valid && checks_ok;
  std::cout << c.name << " pass=" << j["pass"] << " validation=" << valid
            << " checks=" << checks_ok << " errors=" << ctx.errors.Count()
            << '\n';
  return j;
}
int main(int argc, char** argv) {
  if (argc != 2) return 2;
  const std::vector<Case> cases = {
      {"indirect_tail_exposes_unregistered_leaf",
       {li3, li4, ret, li3, 0x4e800420, li4, ret},
       {{0, 12}},
       {{12, 12}, {16, 12}, {20, 20}, {24, 20}},
       {},
       {}},
      {"registered_indirect_tail_exposes_leaf",
       {li3, 0x4e800420, li4, ret},
       {{0, 8}},
       {{0, 0}, {4, 0}, {8, 8}, {12, 8}},
       {},
       {}},
      {"indirect_tail_with_later_direct_reference",
       {li3, li4, ret, li3, 0x4e800420, li4, ret, b(28, 20)},
       {{0, 12}},
       {{12, 12}, {16, 12}, {20, 20}, {24, 20}, {28, 28}},
       {},
       {}},
      {"conditional_bnectr_keeps_fallthrough",
       {li3, li4, ret, li3, 0x4c820420, li4, ret},
       {{0, 12}},
       {{12, 12}, {16, 12}, {20, 12}, {24, 12}},
       {20},
       {}},
      {"conditional_beqctr_keeps_fallthrough",
       {li3, li4, ret, li3, 0x4d820420, li4, ret},
       {{0, 12}},
       {{12, 12}, {16, 12}, {20, 12}, {24, 12}},
       {20},
       {}},
      {"bctrl_call_keeps_fallthrough",
       {li3, li4, ret, li3, 0x4e800421, li4, ret},
       {{0, 12}},
       {{12, 12}, {16, 12}, {20, 12}, {24, 12}},
       {20},
       {}},
      {"conditional_bctrl_call_keeps_fallthrough",
       {li3, li4, ret, li3, 0x4d820421, li4, ret},
       {{0, 12}},
       {{12, 12}, {16, 12}, {20, 12}, {24, 12}},
       {20},
       {}},
      {"ordinary_indirect_tail_remains_owned",
       {li3, li4, ret, li3, 0x4e800420},
       {{0, 12}},
       {{12, 12}, {16, 12}},
       {16},
       {}},
      {"registered_switch_cases_retain_owner",
       {li3, 0x4e800420, li3, ret, li4, ret, li3, ret},
       {{0, 24}},
       {{0, 0}, {4, 0}, {8, 0}, {12, 0}, {16, 0}, {20, 0}, {24, 24}, {28, 24}},
       {8, 16},
       {},
       {{4, {8, 16}}}},
      {"unregistered_switch_cases_retain_owner",
       {li3, li4, ret, li3, 0x4e800420, li3, ret, li4, ret, li3, ret},
       {{0, 12}},
       {{12, 12}, {16, 12}, {20, 12}, {24, 12}, {28, 12}, {32, 12}, {36, 36}, {40, 36}},
       {20, 28},
       {},
       {{16, {20, 28}}}},
      {"registered_out_of_line_ctr_path_preserves_owner",
       {0x2c030000, beq(4, 16), li3, ret, li4, 0x4e800420, li3, ret},
       {{0, 24}},
       {{0, 0}, {4, 0}, {8, 0}, {12, 0}, {16, 0}, {20, 0}, {24, 24}, {28, 24}},
       {16},
       {}},

      {"backward_exit_exposes_unregistered_leaf",
       {li3, li4, ret, li3, b(16, 4), li4, ret, b(28, 20)},
       {{0, 12}},
       {{20, 20}, {24, 20}, {28, 28}},
       {},
       {}},
      {"registered_owner_crosses_partition_then_new_leaf",
       {0x2c030000, beq(4, 16), li3, ret, li4, b(20, 8), li3, ret, b(32, 24)},
       {{0, 24}},
       {{8, 0}, {16, 0}, {20, 0}, {24, 24}, {28, 24}, {32, 32}},
       {16},
       {}},
      {"post_return_block_retains_single_owner",
       {0x2c030000, beq(4, 16), li3, ret, li4, b(20, 8)},
       {{0, 24}},
       {{0, 0}, {4, 0}, {8, 0}, {12, 0}, {16, 0}, {20, 0}},
       {16},
       {}},
      {"own_entry_recursion",
       {li3, li4, ret, li3, b(16, 12)},
       {{0, 12}},
       {{12, 12}, {16, 12}},
       {16},
       {}},
      {"interior_backward_unconditional_loop",
       {li3, li4, ret, li3, li4, b(20, 16)},
       {{0, 12}},
       {{12, 12}, {16, 12}, {20, 12}},
       {16, 20},
       {}},
      {"known_forward_tail_retains_next_leaf",
       {li3, li4, ret, li3, b(16, 28), li4, ret, li3, ret},
       {{0, 12}, {28, 8}},
       {{12, 12}, {16, 12}, {20, 20}, {24, 20}, {28, 28}},
       {},
       {}},
      {"unknown_forward_branch_remains_internal",
       {li3, li4, ret, li3, b(16, 28), li4, ret, li3, ret},
       {{0, 12}},
       {{12, 12}, {16, 12}, {28, 12}, {32, 12}},
       {28},
       {20, 24}},
      {"backward_call_keeps_fallthrough",
       {li3, li4, ret, li3, b(16, 0) | 1u, li4, ret},
       {{0, 12}},
       {{12, 12}, {16, 12}, {20, 12}, {24, 12}},
       {20},
       {}},
      {"conditional_backward_edge_keeps_fallthrough",
       {li3, li4, ret, li3, 0x2c030000, beq(20, 12), li4, ret},
       {{0, 12}},
       {{12, 12}, {16, 12}, {20, 12}, {24, 12}, {28, 12}},
       {24},
       {}},
      {"plain_leaf",
       {li3, li4, ret, li3, ret},
       {{0, 12}},
       {{12, 12}, {16, 12}},
       {},
       {}},
  };
  json results = json::array();
  bool ok = true;
  for (const auto& c : cases) {
    auto result = run(c);
    ok = result["pass"].get<bool>() && ok;
    results.push_back(std::move(result));
  }
  std::ofstream(argv[1]) << results.dump(2) << '\n';
  return ok ? 0 : 1;
}
