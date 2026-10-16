#pragma once

#include "../core/guest_ui_memory.h"

#include <algorithm>

namespace brawlers::ui {

struct MenuTextResult {
  std::optional<std::string> value;
  const char* reason = "none";
  ReadError error = ReadError::None;
  uint64_t address = 0;
};

// Event-time lookup for verified plain-text captions in bank zero. Callers must
// establish the exact resource ID through the feature's native text consumer.
// 823B60B8 selects a descriptor; 8212ECF0 resolves its relative string offset.
// Final entries are deliberately unsupported: no allocation extent is proven.
// Optional detail fields may accept a verified empty string; names and labels
// retain the default rejection. This never relaxes ownership or range checks.
inline MenuTextResult ReadMenuText(const GuestUiMemory& memory, uint32_t id,
                                   bool allow_empty = false) {
  MenuTextResult result;
  const auto fail = [&](const char* reason, uint64_t address) {
    result.reason = reason;
    result.address = address;
    return result;
  };
  const auto u32 = [&](uint64_t address, const char* reason) -> std::optional<uint32_t> {
    if (address > UINT32_MAX - 3) {
      result.error = ReadError::AddressOverflow;
      fail(reason, address);
      return std::nullopt;
    }
    auto value = memory.U32BE(static_cast<uint32_t>(address), &result.error);
    if (!value)
      fail(reason, address);
    return value;
  };
  const auto u8 = [&](uint64_t address, const char* reason) -> std::optional<uint8_t> {
    if (address > UINT32_MAX) {
      result.error = ReadError::AddressOverflow;
      fail(reason, address);
      return std::nullopt;
    }
    auto value = memory.U8(static_cast<uint32_t>(address), &result.error);
    if (!value)
      fail(reason, address);
    return value;
  };
  const auto provider = u32(0x829CEB78, "provider_anchor");
  if (!provider) return result;
  if (!*provider) return fail("provider_null", 0x829CEB78);
  const auto table = u32(*provider, "provider_table_read");
  if (!table) return result;
  if (*table != 0x82036508) return fail("provider_table", *provider);
  const auto banks = u8(uint64_t{*provider} + 0xC, "bank_count_read");
  if (!banks) return result;
  if (!*banks) return fail("bank_count", uint64_t{*provider} + 0xC);
  const auto bank = u32(uint64_t{*provider} + 8, "bank_pointer_read");
  if (!bank) return result;
  if (!*bank) return fail("bank_null", uint64_t{*provider} + 8);
  const auto descriptors = u8(uint64_t{*bank} + 0x60, "descriptor_count_read");
  if (!descriptors) return result;
  if (!*descriptors || *descriptors > 8)
    return fail("descriptor_count", uint64_t{*bank} + 0x60);

  for (uint32_t slot = 0; slot < *descriptors; ++slot) {
    const uint64_t descriptor = uint64_t{*bank} + slot * 0xC;
    const auto resource = u32(descriptor, "resource_pointer_read");
    if (!resource) return result;
    if (!*resource) continue;  // Native null descriptors contribute zero entries.
    const uint64_t base = *resource;
    const auto magic1 = u32(base, "resource_magic_read");
    if (!magic1) return result;
    const auto magic2 = u32(base + 4, "resource_magic_tail_read");
    if (!magic2) return result;
    if (*magic1 != 0x70617374 || *magic2 != 0x7274626C)
      return fail("resource_magic", base);
    const auto flags = u8(base + 0xB, "resource_flags_read");
    if (!flags) return result;
    if (!(*flags & 1)) return fail("resource_flags", base + 0xB);
    const auto count = u32(base + 0xC, "entry_count_read");
    if (!count) return result;
    if (id >= *count) {
      id -= *count;
      continue;
    }
    const auto offsets = u32(base + 0x10, "offset_table_header_read");
    if (!offsets) return result;
    const auto auxiliary = u32(base + 0x14, "auxiliary_table_header_read");
    if (!auxiliary) return result;
    if (*offsets < 0x20 || *offsets % 16)
      return fail("offset_table_alignment", base + 0x10);
    if (*auxiliary < 0x20 || *auxiliary % 16)
      return fail("auxiliary_table_alignment", base + 0x14);
    const uint64_t offset_end = uint64_t{*offsets} + uint64_t{*count} * 4;
    const uint64_t auxiliary_end = uint64_t{*auxiliary} + uint64_t{*count} * 2;
    if (base + std::max(offset_end, auxiliary_end) > uint64_t{UINT32_MAX} + 1)
      return fail("resource_tables_overflow", base);
    const auto bound_offsets = u32(descriptor + 8, "offset_table_pointer_read");
    if (!bound_offsets) return result;
    if (*bound_offsets != base + *offsets)
      return fail("offset_table_pointer", descriptor + 8);
    const auto bound_auxiliary = u32(descriptor + 4, "auxiliary_table_pointer_read");
    if (!bound_auxiliary) return result;
    if (*bound_auxiliary != base + *auxiliary)
      return fail("auxiliary_table_pointer", descriptor + 4);
    if (id == *count - 1) return fail("final_entry_without_bound", base + 0xC);
    const uint64_t entry = base + *offsets + uint64_t{id} * 4;
    const auto start = u32(entry, "text_offset_read");
    if (!start) return result;
    const auto end = u32(entry + 4, "next_text_offset_read");
    if (!end) return result;
    if (*start % 2 || *end % 2 || *start < std::max(offset_end, auxiliary_end) ||
        *end <= *start)
      return fail("text_offset_bounds", entry);
    if (base + *end > uint64_t{UINT32_MAX} + 1)
      return fail("text_address_overflow", base + *end);
    const auto text = memory.Text(static_cast<uint32_t>(base + *start),
                                 std::min<size_t>((*end - *start) / 2, 8192));
    if (!text.value) {
      result.error = text.error;
      return fail("text_read", text.address);
    }
    if (!allow_empty && text.value->empty()) return fail("text_empty", base + *start);
    result.value = text.value;
    return result;
  }
  return fail("entry_not_found", *bank);
}

}  // namespace brawlers::ui
