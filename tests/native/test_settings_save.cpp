#include "native_settings_save.h"

#include <cassert>
#include <map>
#include <stdexcept>
#include <vector>

using namespace brawlers::settings;

namespace {
constexpr uint32_t kGlobal = 0x829CC350;
constexpr uint32_t kSingleton = 0x829CA610;
constexpr uint32_t kSlot = 0x829CA40C;
constexpr uint32_t kService = 0x1000;
constexpr uint32_t kStorage = 0x2000;
constexpr uint32_t kDescriptor = 0x3000;
constexpr uint32_t kImage = 0x4000;
const std::array<uint8_t, 6> kFirst{0, 1, 1, 10, 9, 8};
const std::array<uint8_t, 6> kSecond{0, 0, 1, 7, 6, 5};
const std::array<uint8_t, 6> kLatest{0, 0, 0, 4, 3, 2};

class Guest {
 public:
  Guest() {
    for (uint32_t i = 0; i < 0xD9EC; ++i) bytes[kImage + i] = 0;
    Word(kGlobal, kService);
    Word(kService, 0);
    Word(kService + 0xC, 0);
    Word(kService + 0x24, 0);
    Word(kService + 0x1C, 0);
    Word(kSingleton, kDescriptor);
    Word(kSingleton + 4, 0);
    Word(kSingleton + 8, 0);
    bytes[kSingleton + 0xE] = 0;
    Word(kDescriptor, kImage);
    Word(kDescriptor + 4, kImage + 0x10);
    bytes[kDescriptor + 0x108] = 1;
    bytes[kSlot] = 2;
  }
  void Word(uint32_t address, uint32_t value) {
    for (int i = 3; i >= 0; --i) { bytes[address + i] = value & 0xFF; value >>= 8; }
  }
  uint32_t Word(uint32_t address) {
    uint32_t value = 0;
    for (int i = 0; i < 4; ++i) value = value * 256 + bytes.at(address + i);
    return value;
  }
  std::array<uint8_t, 6> Snapshot() {
    std::array<uint8_t, 6> result;
    for (size_t i = 0; i < result.size(); ++i) result[i] = bytes.at(kStorage + 8 + i);
    return result;
  }
  void Consume() {
    consumed.push_back(Snapshot());
    slots.push_back(bytes[kSlot]);
  }
  void Terminal(uint32_t result, bool success, uint32_t flags = 0) {
    pump = [this, result, success, flags](uint32_t& output) {
      Word(kService, 0);
      Word(kService + 0xC, 0);
      Word(kService + 0x24, 0);
      Word(kService + 0x1C, flags);
      Word(kSingleton + 8, 0);
      bytes[kSingleton + 0xE] = success;
      output = result;
    };
  }
  SaveAccess Access() {
    return {
      [&](uint32_t address, void* output, size_t size) {
        auto* target = static_cast<uint8_t*>(output);
        for (size_t i = 0; i < size; ++i) {
          auto found = bytes.find(address + i);
          if (found == bytes.end()) return false;
          target[i] = found->second;
        }
        return true;
      },
      [&](uint32_t address, const void* input, size_t size) {
        if (fail_write) return false;
        auto* source = static_cast<const uint8_t*>(input);
        for (size_t i = 0; i < size; ++i) bytes[address + i] = source[i];
        return true;
      },
      [&](SaveCall call, uint32_t r3, uint32_t r4, uint32_t r5, uint32_t& result) {
        calls.push_back(call);
        if (throw_native) throw std::runtime_error("Synthetic native call exception");
        if (call == SaveCall::Request) {
          assert(r3 == kService && r4 == kStorage && r5 == 0);
          assert(Word(kService + 0xC) == 0);
          Word(kService, 16);
          Word(kService + 0xC, 6);
          Word(kService + 0x24, r4);
          result = 1;
        } else if (call == SaveCall::WriteRam) {
          assert(r3 == kStorage && r4 == kImage + 0x10);
          Consume();
          result = 1;
        } else if (call == SaveCall::DisableContext) {
          assert(r3 == kSingleton && r4 == 0);
          bytes[Word(kSingleton) + 0x108] = 0;
          result = 0;
        } else {
          assert(call == SaveCall::Pump && r3 == kService);
          assert(pump);
          pump(result);
        }
        return true;
      },
      [&] { ++validations; return context_valid; }
    };
  }
  void JobStarted(NativeSettingsSave& save) {
    pump = [&](uint32_t& result) {
      Consume();
      for (uint32_t i = 0; i < 0xDA30; ++i) bytes[0x20000 + i] = 0;
      Word(kSingleton + 8, 0x20000);
      Word(kService, 17);
      result = 1;
    };
    assert(save.Pump().status == SaveStatus::Pending);
  }
  std::map<uint32_t, uint8_t> bytes;
  std::vector<SaveCall> calls;
  std::vector<std::array<uint8_t, 6>> consumed;
  std::vector<uint8_t> slots;
  std::function<void(uint32_t&)> pump;
  bool context_valid = true;
  bool fail_write = false;
  bool throw_native = false;
  size_t validations = 0;
};

void CoalescedLifetime() {
  Guest guest;
  NativeSettingsSave save(guest.Access());
  assert(save.Init(kStorage));
  assert(guest.Word(kStorage) == 0x82017A08 && guest.Word(kStorage + 4) == kStorage + 8);
  assert(save.Idle());
  assert(save.Enqueue(1, kFirst).status == SaveStatus::Queued);
  assert(save.Pump().status == SaveStatus::Requested);
  assert(guest.Snapshot() == kFirst && !save.Idle());
  assert(save.Enqueue(2, kSecond).status == SaveStatus::Queued);
  assert(save.Enqueue(3, kLatest).status == SaveStatus::Queued);
  assert(guest.Snapshot() == kFirst);  // Queuing must never mutate the borrowed buffer.
  guest.JobStarted(save);
  guest.Terminal(2, true);
  auto result = save.Pump();
  assert(result.status == SaveStatus::FileComplete && result.generation == 1);
  assert(!save.Idle());
  assert(save.Pump().status == SaveStatus::Requested);
  assert(guest.Snapshot() == kLatest);
  guest.JobStarted(save);
  guest.Terminal(2, true);
  result = save.Pump();
  assert(result.status == SaveStatus::FileComplete && result.generation == 3);
  assert(save.Idle());
  assert(guest.consumed.size() == 2 && guest.consumed[1] == kLatest);
  assert(guest.validations == guest.calls.size());
  const auto calls = guest.calls.size();
  assert(save.Pump().status == SaveStatus::Idle && guest.calls.size() == calls);
}

void NativeRetryOwnsDestination() {
  Guest guest;
  NativeSettingsSave save(guest.Access());
  assert(save.Init(kStorage));
  save.Enqueue(1, kFirst);
  save.Pump();
  guest.JobStarted(save);
  save.Enqueue(2, kLatest);
  // Failed job has been freed, but the native error UI still owns the adapter.
  guest.pump = [&](uint32_t& result) {
    guest.Word(kSingleton + 8, 0);
    guest.bytes[kSingleton + 0xE] = 0;
    guest.Word(kService, 1);
    result = 1;
  };
  assert(save.Pump().status == SaveStatus::Pending);
  assert(guest.Snapshot() == kFirst && !save.Idle());
  // Native destination selection temporarily uses another valid descriptor.
  guest.Word(0x18000, 0x19000);
  guest.Word(0x18004, 0x19010);
  guest.bytes[0x18108] = 1;
  guest.Word(kSingleton, 0x18000);
  assert(save.Pump().status == SaveStatus::Pending);
  guest.Word(kSingleton, kDescriptor);
  guest.bytes[kSlot] = 0;  // Explicit native retry selection may change the slot.
  guest.JobStarted(save);
  assert(guest.consumed.size() == 2 && guest.consumed[1] == kFirst);
  assert(guest.slots[0] == 2 && guest.slots[1] == 0);
  guest.Terminal(2, true);
  assert(save.Pump().status == SaveStatus::FileComplete);
  assert(save.Pump().status == SaveStatus::Requested);
  assert(guest.Snapshot() == kLatest);
}

void BusyAndRamOnly() {
  Guest guest;
  NativeSettingsSave save(guest.Access());
  assert(save.Init(kStorage));
  save.Enqueue(1, kFirst);
  guest.Word(kService + 0xC, 5);
  guest.Word(kService + 0x24, 0x1234);
  assert(save.Pump().status == SaveStatus::Busy);
  assert(guest.calls.empty() && guest.Word(kService + 0x24) == 0x1234);
  guest.Word(kService + 0xC, 0);
  guest.Word(kService + 0x24, 0);
  guest.Word(kSingleton + 8, 0x20000);
  guest.Word(0x20000 + 0xDA28, 1);
  assert(save.Pump().status == SaveStatus::Busy && guest.calls.empty());
  guest.Word(kSingleton + 8, 0);
  guest.bytes[kDescriptor + 0x108] = 0;
  const auto result = save.Pump();
  assert(result.status == SaveStatus::RamOnly && result.generation == 1);
  assert(save.Idle() && guest.calls == std::vector<SaveCall>{SaveCall::WriteRam});
}

void TerminalOutcomes() {
  for (uint32_t terminal : {2u, 3u, 4u}) {
    Guest guest;
    NativeSettingsSave save(guest.Access());
    assert(save.Init(kStorage));
    save.Enqueue(1, kFirst);
    save.Pump();
    guest.Terminal(terminal, true);
    const auto result = save.Pump();
    // A result/outcome flag without an observed owned job is not write proof.
    assert(result.status == SaveStatus::Abandoned && result.native_result == terminal);
    assert(save.Idle());
  }
  Guest guest;
  NativeSettingsSave save(guest.Access());
  assert(save.Init(kStorage));
  save.Enqueue(1, kFirst);
  save.Pump();
  guest.JobStarted(save);
  guest.Terminal(2, false, 1);
  assert(save.Pump().status == SaveStatus::Abandoned);
  assert(guest.calls.back() == SaveCall::DisableContext);
  assert(guest.bytes[kDescriptor + 0x108] == 0 && guest.Word(kService + 0x1C) == 0);
  assert(save.Idle());
}

void FaultRecoveryAndRelease() {
  {
    Guest guest;
    NativeSettingsSave save(guest.Access());
    assert(save.Init(kStorage) && save.CanRelease());
    save.Enqueue(1, kFirst);
    assert(!save.AbandonQueued());  // Normal queued changes cannot be discarded.
    guest.context_valid = false;
    assert(save.Pump().status == SaveStatus::Fault);
    assert(save.CanRelease() && !save.Idle());
    assert(!save.RetryValidation());  // No native request was submitted.
    assert(save.AbandonQueued() && save.Idle());
    assert(save.fault() == "settings_save_native_context");
    assert(save.Enqueue(2, kLatest).status == SaveStatus::Fault);
    assert(guest.calls.empty());
  }
  {
    Guest guest;
    NativeSettingsSave save(guest.Access());
    assert(save.Init(kStorage));
    save.Enqueue(1, kFirst);
    save.Pump();
    guest.JobStarted(save);
    guest.context_valid = false;
    assert(save.Pump().status == SaveStatus::Fault);
    assert(!save.CanRelease() && !save.AbandonQueued() && !save.RetryValidation());
    guest.context_valid = true;
    const auto calls = guest.calls.size();
    assert(save.RetryValidation() && save.fault().empty());
    assert(guest.calls.size() == calls);  // Recovery validates; it does not pump.
    guest.Terminal(2, true);
    assert(save.Pump().status == SaveStatus::FileComplete);
    assert(save.Idle() && save.CanRelease());
  }
  {
    Guest guest;
    NativeSettingsSave save(guest.Access());
    assert(save.Init(kStorage));
    save.Enqueue(1, kFirst);
    save.Pump();
    guest.JobStarted(save);
    guest.bytes.erase(kImage + 0x9000);
    assert(save.Pump().status == SaveStatus::Fault);
    assert(!save.RetryValidation());
    guest.bytes[kImage + 0x9000] = 0;
    assert(save.RetryValidation());
    guest.Terminal(2, true);
    assert(save.Pump().status == SaveStatus::FileComplete);
  }
  {
    Guest guest;
    NativeSettingsSave save(guest.Access());
    assert(save.Init(kStorage));
    save.Enqueue(1, kFirst);
    save.Pump();
    guest.throw_native = true;
    assert(save.Pump().status == SaveStatus::Fault);
    guest.throw_native = false;
    assert(save.fault() == "settings_save_call_exception");
    assert(!save.RetryValidation() && !save.CanRelease() && !save.AbandonQueued());
    // A separate original owner may have completed cleanup. Release requires
    // new evidence of every retained reference being absent, not a fault reset.
    guest.Word(kService, 0);
    guest.Word(kService + 0xC, 0);
    guest.Word(kService + 0x24, 0);
    assert(save.CanRelease() && save.AbandonQueued() && save.Idle());
    guest.bytes.erase(kService + 0x24);
    assert(!save.CanRelease());
  }
  {
    Guest guest;
    NativeSettingsSave save(guest.Access());
    assert(save.Init(kStorage));
    save.Enqueue(1, kFirst);
    guest.fail_write = true;
    assert(save.Pump().status == SaveStatus::Fault);
    assert(!save.RetryValidation());
    assert(save.CanRelease() && save.AbandonQueued() && save.Idle());
  }
}

void OwnerDestruction() {
  {
    Guest guest;
    NativeSettingsSave save(guest.Access());
    assert(save.Init(kStorage));
    save.Enqueue(1, kFirst);
    assert(save.OwnerClosed() == 1);
    assert(save.Idle() && save.CanRelease());
    assert(save.OwnerClosed() == 0);
    assert(save.Enqueue(2, kLatest).reason == "settings_save_owner_closed");
    assert(save.Pump().status == SaveStatus::Idle && guest.calls.empty());
  }
  {
    Guest guest;
    NativeSettingsSave save(guest.Access());
    assert(save.Init(kStorage));
    save.Enqueue(1, kFirst);
    assert(save.Pump().status == SaveStatus::Requested);
    guest.JobStarted(save);
    save.Enqueue(2, kSecond);
    save.Enqueue(3, kLatest);
    assert(save.OwnerClosed() == 3);
    assert(!save.Idle() && !save.CanRelease());
    assert(guest.Snapshot() == kFirst);
    // The detached owner can recover its borrowed operation, and native retry
    // still owns destination changes. No queued generation can follow it.
    guest.context_valid = false;
    assert(save.Pump().status == SaveStatus::Fault);
    assert(save.OwnerClosed() == 0 && !save.AbandonQueued());
    guest.context_valid = true;
    assert(save.RetryValidation());
    guest.pump = [&](uint32_t& result) {
      guest.Word(kSingleton + 8, 0);
      guest.bytes[kSingleton + 0xE] = 0;
      guest.Word(kService, 1);
      result = 1;
    };
    assert(save.Pump().status == SaveStatus::Pending);
    guest.bytes[kSlot] = 0;
    guest.JobStarted(save);
    assert(guest.consumed.size() == 2 && guest.consumed[1] == kFirst);
    guest.Terminal(2, true);
    const auto result = save.Pump();
    assert(result.status == SaveStatus::FileComplete && result.generation == 1);
    assert(save.Idle() && save.CanRelease());
    const auto calls = guest.calls.size();
    assert(save.Pump().status == SaveStatus::Idle && guest.calls.size() == calls);
    assert(save.Enqueue(4, kLatest).reason == "settings_save_owner_closed");
  }
  {
    Guest guest;
    NativeSettingsSave save(guest.Access());
    assert(save.Init(kStorage));
    save.Enqueue(1, kFirst);
    guest.context_valid = false;
    assert(save.Pump().status == SaveStatus::Fault && guest.calls.empty());
    // A missing ownership field cannot grant release, even before Request.
    guest.bytes.erase(kService + 0x24);
    assert(save.OwnerClosed() == 0 && !save.Idle() && !save.CanRelease());
    guest.Word(kService + 0x24, 0);
    assert(save.OwnerClosed() == 1 && save.Idle() && save.CanRelease());
    assert(save.fault() == "settings_save_native_context");
    assert(save.OwnerClosed() == 0 && guest.calls.empty());
  }
}

void InvalidRequestsAndFaults() {
  Guest guest;
  NativeSettingsSave save(guest.Access());
  assert(save.Enqueue(1, kFirst).status == SaveStatus::Fault);
  assert(save.Init(kStorage));
  assert(!save.Init(kStorage));
  auto invalid = kFirst;
  invalid[3] = 0;
  assert(save.Enqueue(1, invalid).status == SaveStatus::Fault);
  assert(save.Enqueue(1, kFirst).status == SaveStatus::Queued);
  assert(save.Enqueue(1, kLatest).status == SaveStatus::Fault);
  assert(save.Enqueue(0, kLatest).status == SaveStatus::Fault);
  assert(save.Pump().status == SaveStatus::Requested);
  guest.Word(kService + 0x24, 0x9999);
  const auto calls = guest.calls.size();
  assert(save.Pump().status == SaveStatus::Fault);
  assert(!save.Idle() && !save.fault().empty() && guest.calls.size() == calls);
  Guest other;
  NativeSettingsSave blocked(other.Access());
  assert(blocked.Init(kStorage));
  blocked.Enqueue(1, kFirst);
  other.context_valid = false;
  assert(blocked.Pump().status == SaveStatus::Fault && other.calls.empty());
  assert(!blocked.Idle());  // A fault never authorizes storage release.
  Guest missing;
  missing.bytes.erase(kDescriptor + 0x108);
  NativeSettingsSave unreadable(missing.Access());
  assert(!unreadable.Init(kStorage) && missing.calls.empty());
  Guest wrong;
  wrong.Word(kDescriptor + 4, kImage + 0x14);
  NativeSettingsSave malformed(wrong.Access());
  assert(!malformed.Init(kStorage));
  Guest unwritable;
  unwritable.fail_write = true;
  NativeSettingsSave failed(unwritable.Access());
  assert(!failed.Init(kStorage));
  Guest changed;
  NativeSettingsSave corrupted(changed.Access());
  assert(corrupted.Init(kStorage));
  corrupted.Enqueue(1, kFirst);
  corrupted.Pump();
  changed.bytes[kStorage + 11] = 1;
  const auto prior = changed.calls.size();
  assert(corrupted.Pump().status == SaveStatus::Fault);
  assert(corrupted.fault() == "settings_save_snapshot_changed");
  assert(changed.calls.size() == prior);
  Guest hole;
  NativeSettingsSave unreadable_image(hole.Access());
  assert(unreadable_image.Init(kStorage));
  unreadable_image.Enqueue(1, kFirst);
  hole.bytes.erase(kImage + 0x9000);
  assert(unreadable_image.Pump().status == SaveStatus::Fault);
  assert(unreadable_image.fault() == "settings_save_image_range" && hole.calls.empty());
  Guest job_hole;
  NativeSettingsSave unreadable_job(job_hole.Access());
  assert(unreadable_job.Init(kStorage));
  unreadable_job.Enqueue(1, kFirst);
  unreadable_job.Pump();
  job_hole.JobStarted(unreadable_job);
  job_hole.bytes.erase(0x20000 + 0x9000);
  const auto before_prepare = job_hole.calls.size();
  assert(unreadable_job.Pump().status == SaveStatus::Fault);
  assert(unreadable_job.fault() == "settings_save_job_range");
  assert(job_hole.calls.size() == before_prepare);
}
}

int main() {
  CoalescedLifetime();
  NativeRetryOwnsDestination();
  BusyAndRamOnly();
  TerminalOutcomes();
  InvalidRequestsAndFaults();
  FaultRecoveryAndRelease();
  OwnerDestruction();
}
