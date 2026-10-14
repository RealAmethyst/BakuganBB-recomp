#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace brawlers::ui {

using ReadBytes = std::function<bool(uint32_t, void*, size_t)>;

enum class ReadError {
  None,
  NullAddress,
  UnalignedAddress,
  AddressOverflow,
  ReadFailed,
  UnterminatedText,
  InvalidSurrogate,
  InvalidLimit,
  CallbackException,
};

constexpr const char* ReadErrorName(ReadError error) {
  switch (error) {
    case ReadError::None: return "none";
    case ReadError::NullAddress: return "null_address";
    case ReadError::UnalignedAddress: return "unaligned_address";
    case ReadError::AddressOverflow: return "address_overflow";
    case ReadError::ReadFailed: return "read_failed";
    case ReadError::UnterminatedText: return "unterminated_text";
    case ReadError::InvalidSurrogate: return "invalid_surrogate";
    case ReadError::InvalidLimit: return "invalid_limit";
    case ReadError::CallbackException: return "callback_exception";
  }
  return "unknown";
}

struct TextResult {
  std::optional<std::string> value;
  ReadError error = ReadError::None;
  // Wide enough to identify an attempted read just beyond the guest address space.
  uint64_t address = 0;
};

class GuestUiMemory {
 public:
  explicit GuestUiMemory(ReadBytes read_bytes) : read_bytes_(std::move(read_bytes)) {}

  std::optional<uint8_t> U8(uint32_t address, ReadError* error = nullptr) const {
    return ReadUnsigned<uint8_t>(address, error);
  }

  std::optional<uint16_t> U16BE(uint32_t address, ReadError* error = nullptr) const {
    return ReadUnsigned<uint16_t>(address, error);
  }

  std::optional<uint32_t> U32BE(uint32_t address, ReadError* error = nullptr) const {
    return ReadUnsigned<uint32_t>(address, error);
  }

  // Reads only this supplied pointer. max_units includes the terminating UTF16
  // zero; malformed, inaccessible or unterminated text never returns a prefix.
  // Text and markup are preserved literally after strict UTF16BE conversion.
  TextResult Text(uint32_t address, size_t max_units = 8192) const {
    if (!address)
      return {std::nullopt, ReadError::NullAddress, address};
    if (address % 2)
      return {std::nullopt, ReadError::UnalignedAddress, address};
    if (!max_units || max_units > (uint64_t{1} << 31))
      return {std::nullopt, ReadError::InvalidLimit, address};

    std::string text;
    for (size_t index = 0; index < max_units; ++index) {
      const uint64_t current = address + uint64_t{index} * 2;
      ReadError error;
      const auto first = ReadUnsigned<uint16_t>(current, &error);
      if (!first)
        return {std::nullopt, error, current};
      if (!*first)
        return {std::move(text), ReadError::None, address};

      uint32_t codepoint = *first;
      if (codepoint >= 0xDC00 && codepoint <= 0xDFFF)
        return {std::nullopt, ReadError::InvalidSurrogate, current};
      if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
        if (++index == max_units)
          return {std::nullopt, ReadError::InvalidSurrogate, current + 2};
        const auto second = ReadUnsigned<uint16_t>(current + 2, &error);
        if (!second)
          return {std::nullopt, error, current + 2};
        if (*second < 0xDC00 || *second > 0xDFFF)
          return {std::nullopt, ReadError::InvalidSurrogate, current + 2};
        codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (*second - 0xDC00);
      }
      AppendUtf8(text, codepoint);
    }
    return {std::nullopt, ReadError::UnterminatedText, address + uint64_t{max_units} * 2};
  }

 private:
  template <typename T>
  std::optional<T> ReadUnsigned(uint64_t address, ReadError* error) const {
    const auto fail = [error](ReadError reason) -> std::optional<T> {
      if (error)
        *error = reason;
      return std::nullopt;
    };
    if (!address)
      return fail(ReadError::NullAddress);
    constexpr uint64_t last_address = std::numeric_limits<uint32_t>::max();
    if (address > last_address || sizeof(T) - 1 > last_address - address)
      return fail(ReadError::AddressOverflow);
    if (address % sizeof(T))
      return fail(ReadError::UnalignedAddress);

    uint8_t bytes[sizeof(T)]{};
    try {
      if (!read_bytes_ || !read_bytes_(static_cast<uint32_t>(address), bytes, sizeof(bytes)))
        return fail(ReadError::ReadFailed);
    } catch (...) {
      return fail(ReadError::CallbackException);
    }
    uint32_t value = 0;
    for (uint8_t byte : bytes)
      value = (value << 8) | byte;
    if (error)
      *error = ReadError::None;
    return static_cast<T>(value);
  }

  static void AppendUtf8(std::string& text, uint32_t codepoint) {
    if (codepoint <= 0x7F) {
      text.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FF) {
      text.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
      text.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0xFFFF) {
      text.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
      text.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
      text.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
      text.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
      text.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
      text.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
      text.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
  }

  ReadBytes read_bytes_;
};

}  // namespace brawlers::ui
