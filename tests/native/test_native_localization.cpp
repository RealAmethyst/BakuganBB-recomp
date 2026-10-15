// Independently authored text resources; no game or runtime is loaded.
#include "../../src/game/native_localization.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include <map>
#include <string_view>
#include <vector>

using brawlers::ui::GuestUiMemory;
using brawlers::ui::ReadMenuText;

namespace {
struct Fixture {
  std::map<uint32_t, uint8_t> bytes;
  std::vector<uint32_t> reads;
  void Word(uint32_t address, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i)
      bytes[address + i] = static_cast<uint8_t>(value >> ((3 - i) * 8));
  }
  void Unit(uint32_t address, uint16_t value) {
    bytes[address] = static_cast<uint8_t>(value >> 8);
    bytes[address + 1] = static_cast<uint8_t>(value);
  }
  void Resource(uint32_t base, uint32_t descriptor) {
    Word(descriptor, base);
    Word(descriptor + 4, base + 0x30);
    Word(descriptor + 8, base + 0x20);
    Word(base, 0x70617374);
    Word(base + 4, 0x7274626C);
    bytes[base + 0xB] = 1;
    Word(base + 0xC, 3);
    Word(base + 0x10, 0x20);
    Word(base + 0x14, 0x30);
    Word(base + 0x20, 0x40);
    Word(base + 0x24, 0x48);
    Word(base + 0x28, 0x50);
    Unit(base + 0x40, 'A');
    Unit(base + 0x42, 0xE9);
    Unit(base + 0x44, 0);
    Unit(base + 0x48, 'B');
    Unit(base + 0x4A, 0);
  }
  Fixture() {
    Word(0x829CEB78, 0x1000);
    Word(0x1000, 0x82036508);
    Word(0x1008, 0x2000);
    bytes[0x100C] = 1;
    bytes[0x2060] = 1;
    Resource(0x3000, 0x2000);
  }
  GuestUiMemory Reader() {
    return GuestUiMemory([this](uint32_t address, void* output, size_t length) {
      reads.push_back(address);
      for (size_t i = 0; i < length; ++i) {
        const auto found = bytes.find(address + static_cast<uint32_t>(i));
        if (found == bytes.end()) return false;
        static_cast<uint8_t*>(output)[i] = found->second;
      }
      return true;
    });
  }
  void Reject(std::string_view reason, uint32_t id = 0) {
    const auto result = ReadMenuText(Reader(), id);
    assert(!result.value && result.reason == reason);
  }
};
}  // namespace

int main() {
  Fixture valid;
  assert(ReadMenuText(valid.Reader(), 0).value == "A\xC3\xA9");
  assert(ReadMenuText(valid.Reader(), 1).value == "B");
  valid.Reject("final_entry_without_bound", 2);
  valid.Reject("entry_not_found", 3);

  Fixture multiple;
  multiple.bytes[0x2060] = 3;
  multiple.Word(0x200C, 0);  // Null descriptor is skipped, as in the native getter.
  multiple.Resource(0x4000, 0x2018);
  multiple.Unit(0x4040, 'C');
  assert(ReadMenuText(multiple.Reader(), 3).value == "C\xC3\xA9");

  const auto bad_word = [](uint32_t address, uint32_t value, std::string_view reason) {
    Fixture fixture;
    fixture.Word(address, value);
    fixture.Reject(reason);
  };
  bad_word(0x829CEB78, 0, "provider_null");
  bad_word(0x1000, 0x12345678, "provider_table");
  bad_word(0x1008, 0, "bank_null");
  bad_word(0x3000, 0, "resource_magic");
  bad_word(0x3010, 0x21, "offset_table_alignment");
  bad_word(0x3014, 0, "auxiliary_table_alignment");
  bad_word(0x2008, 0x3030, "offset_table_pointer");
  bad_word(0x2004, 0x3040, "auxiliary_table_pointer");
  bad_word(0x300C, 0xFFFFFFFF, "resource_tables_overflow");
  bad_word(0x3020, 0x41, "text_offset_bounds");
  bad_word(0x3020, 0x20, "text_offset_bounds");
  bad_word(0x3024, 0x40, "text_offset_bounds");
  bad_word(0x3024, 0xFFFFF000, "text_address_overflow");
  for (const uint8_t count : {0, 9, 255}) {
    Fixture fixture;
    fixture.bytes[0x2060] = count;
    fixture.Reject("descriptor_count");
  }
  Fixture no_banks;
  no_banks.bytes[0x100C] = 0;
  no_banks.Reject("bank_count");
  Fixture no_flag;
  no_flag.bytes[0x300B] = 0;
  no_flag.Reject("resource_flags");
  Fixture near_end;
  near_end.Word(0x829CEB78, 0xFFFFFFFC);
  near_end.Word(0xFFFFFFFC, 0x82036508);
  near_end.Reject("bank_count_read");
  Fixture inaccessible;
  inaccessible.bytes.erase(0x3027);
  inaccessible.Reject("next_text_offset_read");
  Fixture empty;
  empty.Unit(0x3040, 0);
  empty.Reject("text_empty");
  Fixture surrogate;
  surrogate.Unit(0x3040, 0xDC00);
  surrogate.Reject("text_read");
  Fixture unterminated;
  unterminated.Unit(0x3044, 'x');
  unterminated.Unit(0x3046, 'y');
  unterminated.Reject("text_read");
  assert(unterminated.reads.back() == 0x3046);  // Never read the next string.
  Fixture budget;
  budget.Word(0x3024, 0x5000);
  for (uint32_t i = 0; i < 8192; ++i) budget.Unit(0x3040 + i * 2, 'x');
  budget.Reject("text_read");
  assert(budget.reads.back() == 0x3040 + 8191 * 2);
  std::cout << "Native localization bounds and descriptor guards passed\n";
}
