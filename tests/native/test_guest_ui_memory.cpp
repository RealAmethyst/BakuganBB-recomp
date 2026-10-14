// Original synthetic guest bytes. No runtime, game or desktop services are loaded.
#include "guest_ui_memory.h"

#include <cassert>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <vector>

using brawlers::ui::GuestUiMemory;
using brawlers::ui::ReadError;

namespace {

struct Fixture {
  uint32_t base = 0x1000;
  std::vector<uint8_t> bytes;
  size_t calls = 0;
  std::vector<uint32_t> addresses;

  GuestUiMemory Reader() {
    return GuestUiMemory([this](uint32_t address, void* output, size_t count) {
      ++calls;
      addresses.push_back(address);
      if (address < base || uint64_t{address} - base > bytes.size())
        return false;
      const size_t offset = address - base;
      if (count > bytes.size() - offset)
        return false;
      std::memcpy(output, bytes.data() + offset, count);
      return true;
    });
  }

  void Units(std::initializer_list<uint16_t> units) {
    bytes.clear();
    for (uint16_t unit : units) {
      bytes.push_back(static_cast<uint8_t>(unit >> 8));
      bytes.push_back(static_cast<uint8_t>(unit));
    }
  }
};

void ExpectError(Fixture& fixture, ReadError error, size_t limit = 8192) {
  const auto result = fixture.Reader().Text(fixture.base, limit);
  assert(!result.value && result.error == error);
}

}  // namespace

int main() {
  Fixture fixture;
  auto reader = fixture.Reader();
  fixture.bytes = {0x12, 0x34, 0x56, 0x78};
  ReadError error = ReadError::ReadFailed;
  assert(reader.U8(0x1001, &error) == 0x34 && error == ReadError::None);
  assert(reader.U16BE(0x1000) == 0x1234);
  assert(reader.U32BE(0x1000) == 0x12345678);
  const size_t valid_calls = fixture.calls;
  assert(!reader.U8(0, &error) && error == ReadError::NullAddress);
  assert(!reader.U16BE(0x1001, &error) && error == ReadError::UnalignedAddress);
  assert(!reader.U32BE(0x1002, &error) && error == ReadError::UnalignedAddress);
  assert(fixture.calls == valid_calls);
  assert(!reader.U16BE(0x1004, &error) && error == ReadError::ReadFailed);
  fixture.bytes.resize(3);
  assert(!reader.U32BE(0x1000, &error) && error == ReadError::ReadFailed);

  fixture.Units({'A', 0x00E9, 0x20AC, 0xD83D, 0xDE00, 0});
  auto text = reader.Text(0x1000, 6);
  assert(text.value == "A\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80");
  assert(text.error == ReadError::None && text.address == 0x1000);
  ExpectError(fixture, ReadError::UnterminatedText, 5);
  fixture.Units({'<', 'b', '>', '#', '#', '\n', 0});
  assert(reader.Text(0x1000, 7).value == "<b>##\n");
  fixture.Units({0});
  assert(reader.Text(0x1000, 1).value == "");
  fixture.Units({'A', 0});
  fixture.addresses.clear();
  ExpectError(fixture, ReadError::UnterminatedText, 1);
  assert(fixture.addresses == std::vector<uint32_t>{0x1000});
  assert(reader.Text(0x1000, 2).value == "A");
  const size_t before_invalid = fixture.calls;
  assert(reader.Text(0).error == ReadError::NullAddress);
  assert(reader.Text(0x1001).error == ReadError::UnalignedAddress);
  ExpectError(fixture, ReadError::InvalidLimit, 0);
  ExpectError(fixture, ReadError::InvalidLimit, std::numeric_limits<size_t>::max());
  assert(fixture.calls == before_invalid);

  fixture.bytes = {0x00};
  ExpectError(fixture, ReadError::ReadFailed);
  fixture.Units({'A'});
  text = reader.Text(0x1000, 2);
  assert(!text.value && text.error == ReadError::ReadFailed && text.address == 0x1002);
  fixture.Units({0xDC00, 0});
  ExpectError(fixture, ReadError::InvalidSurrogate);
  fixture.Units({0xD800, 'A', 0});
  ExpectError(fixture, ReadError::InvalidSurrogate);
  fixture.Units({0xD800, 0, 0});
  ExpectError(fixture, ReadError::InvalidSurrogate);
  fixture.Units({0xD800, 0xD800, 0});
  ExpectError(fixture, ReadError::InvalidSurrogate);
  fixture.Units({0xD800});
  ExpectError(fixture, ReadError::InvalidSurrogate, 1);
  ExpectError(fixture, ReadError::ReadFailed, 2);
  fixture.Units({0xD800, 0xDC00, 0});
  assert(reader.Text(0x1000, 3).value == "\xF0\x90\x80\x80");
  fixture.Units({0xDBFF, 0xDFFF, 0});
  assert(reader.Text(0x1000, 3).value == "\xF4\x8F\xBF\xBF");
  fixture.Units({0x007F, 0x0080, 0x07FF, 0x0800, 0xD7FF, 0xE000, 0xFFFF, 0});
  assert(reader.Text(0x1000, 8).value ==
         "\x7F\xC2\x80\xDF\xBF\xE0\xA0\x80\xED\x9F\xBF\xEE\x80\x80\xEF\xBF\xBF");

  fixture.base = 0xFFFFFFFE;
  fixture.Units({0});
  assert(reader.Text(fixture.base).value == "");
  fixture.Units({'A'});
  fixture.addresses.clear();
  text = reader.Text(fixture.base, 2);
  assert(!text.value && text.error == ReadError::AddressOverflow && text.address == 0x100000000ULL);
  assert(fixture.addresses == std::vector<uint32_t>{0xFFFFFFFE});
  fixture.Units({0xD800});
  ExpectError(fixture, ReadError::AddressOverflow, 2);
  fixture.base = 0xFFFFFFFC;
  fixture.Units({'Z', 0});
  assert(reader.Text(fixture.base, 2).value == "Z");
  assert(reader.U32BE(fixture.base) == 0x005A0000);
  fixture.base = 0xFFFFFFFF;
  fixture.bytes = {0xFF};
  assert(reader.U8(fixture.base) == 0xFF);

  fixture.base = 0x1000;
  fixture.bytes.assign(8192 * 2, 0);
  for (size_t i = 0; i < 8191; ++i)
    fixture.bytes[i * 2 + 1] = 'x';
  assert(reader.Text(fixture.base).value == std::string(8191, 'x'));
  fixture.bytes.back() = 'x';
  ExpectError(fixture, ReadError::UnterminatedText);
  assert(reader.Text(fixture.base, 8191).error == ReadError::UnterminatedText);

  GuestUiMemory missing({});
  assert(!missing.U8(1, &error) && error == ReadError::ReadFailed);
  assert(missing.Text(2).error == ReadError::ReadFailed);
  GuestUiMemory throwing([](uint32_t, void*, size_t) -> bool {
    throw std::runtime_error("synthetic read failure");
  });
  assert(!throwing.U32BE(4, &error) && error == ReadError::CallbackException);
  assert(throwing.Text(2).error == ReadError::CallbackException);
  GuestUiMemory partial([](uint32_t, void* output, size_t) {
    *static_cast<uint8_t*>(output) = 0;
    return false;
  });
  assert(partial.Text(2).error == ReadError::ReadFailed);
  assert(std::string(brawlers::ui::ReadErrorName(ReadError::InvalidSurrogate)) == "invalid_surrogate");
  std::cout << "PASS: bounded guest UI scalar and strict UTF16BE reader fixtures\n";
}
