#include "native_settings_save.h"

#include <algorithm>
#include <utility>

namespace brawlers::settings {
namespace {
constexpr uint32_t kService = 0x829CC350;
constexpr uint32_t kSingleton = 0x829CA610;
constexpr uint32_t kAdapterTable = 0x82017A08;
constexpr uint32_t kSlot = 0x829CA40C;

bool Address(uint32_t address, uint32_t size) {
  return address != 0 && address % 4 == 0 && address <= UINT32_MAX - (size - 1);
}
}

NativeSettingsSave::NativeSettingsSave(SaveAccess access)
    : access_(std::move(access)), memory_(access_.read) {}

bool NativeSettingsSave::Fail(std::string_view reason) {
  if (fault_.empty()) fault_ = reason;
  return false;
}

SaveUpdate NativeSettingsSave::FaultUpdate() const {
  return {SaveStatus::Fault, inflight_ ? inflight_->generation : 0, 0, fault_};
}

bool NativeSettingsSave::Write(uint32_t address, const void* bytes, size_t size) {
  try {
    if (!access_.write || !access_.write(address, bytes, size)) return Fail("settings_save_write");
    return true;
  } catch (...) { return Fail("settings_save_write_exception"); }
}

bool NativeSettingsSave::WriteWord(uint32_t address, uint32_t value) {
  const std::array<uint8_t, 4> bytes{static_cast<uint8_t>(value >> 24),
      static_cast<uint8_t>(value >> 16), static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value)};
  return Write(address, bytes.data(), bytes.size());
}

bool NativeSettingsSave::Invoke(SaveCall call, uint32_t r3, uint32_t r4,
                                uint32_t r5, uint32_t& result) {
  try {
    if (!access_.validate_context || !access_.validate_context())
      return Fail("settings_save_native_context");
    if (call == SaveCall::Request) request_may_be_borrowed_ = true;
    if (!access_.call || !access_.call(call, r3, r4, r5, result))
      return Fail("settings_save_native_call");
    return true;
  } catch (...) { return Fail("settings_save_call_exception"); }
}

bool NativeSettingsSave::ReadRange(uint32_t address, size_t size, std::string_view reason) {
  std::array<uint8_t, 1024> block{};
  try {
    for (size_t offset = 0; offset < size; offset += block.size()) {
      const auto count = std::min(block.size(), size - offset);
      if (!access_.read(address + static_cast<uint32_t>(offset), block.data(), count))
        return Fail(reason);
    }
    return true;
  } catch (...) { return Fail(reason); }
}

bool NativeSettingsSave::ReadState(State& state) {
  if (storage_) {
    if (memory_.U32BE(storage_) != kAdapterTable || memory_.U32BE(storage_ + 4) != storage_ + 8)
      return Fail("settings_save_adapter_identity");
    if (inflight_) {
      for (size_t i = 0; i < inflight_->values.size(); ++i) {
        if (memory_.U8(storage_ + 8 + static_cast<uint32_t>(i)) != inflight_->values[i])
          return Fail("settings_save_snapshot_changed");
      }
    }
  }
  const auto service = memory_.U32BE(kService);
  if (!service || !Address(*service, 0x3C) || (service_ && service_ != *service))
    return Fail("settings_save_service_identity");
  state.service = *service;
  const auto phase = memory_.U32BE(*service);
  const auto operation = memory_.U32BE(*service + 0xC);
  const auto callback = memory_.U32BE(*service + 0x24);
  const auto flags = memory_.U32BE(*service + 0x1C);
  if (!phase || *phase > 0x15 || !operation || *operation > 7 || !callback || !flags)
    return Fail("settings_save_service_state");
  state.state = *phase;
  state.operation = *operation;
  state.callback = *callback;
  state.flags = *flags;
  const auto descriptor = memory_.U32BE(kSingleton);
  const auto load_job = memory_.U32BE(kSingleton + 4);
  const auto save_job = memory_.U32BE(kSingleton + 8);
  const auto outcome = memory_.U8(kSingleton + 0xE);
  if (!descriptor || !Address(*descriptor, 0x110) || !load_job || !save_job || !outcome || *outcome > 1)
    return Fail("settings_save_singleton_state");
  const auto image = memory_.U32BE(*descriptor);
  const auto payload = memory_.U32BE(*descriptor + 4);
  const auto active = memory_.U8(*descriptor + 0x108);
  const auto slot = memory_.U8(kSlot);
  if (!image || !Address(*image, 0xD9EC) || !payload || *payload != *image + 0x10 ||
      !active || *active > 1 || !slot || *slot > 3)
    return Fail("settings_save_payload_layout");
  state.descriptor = *descriptor;
  state.image = *image;
  state.payload = *payload;
  state.active = *active;
  state.load_job = *load_job;
  state.save_job = *save_job;
  if (state.save_job && inflight_) {
    if (!Address(state.save_job, 0xDA30)) return Fail("settings_save_job_address");
    const auto job_state = memory_.U32BE(state.save_job + 0xDA28);
    if (!job_state || *job_state > 3) return Fail("settings_save_job_state");
    state.save_job_state = *job_state;
  }
  state.outcome = *outcome;
  return true;
}

bool NativeSettingsSave::Init(uint32_t guest_storage) {
  if (initialized_ || !fault_.empty()) return false;
  if (!Address(guest_storage, 16) || !access_.read || !access_.write || !access_.call ||
      !access_.validate_context) return Fail("settings_save_setup");
  State state;
  if (!ReadState(state)) return false;
  // Storage is caller-owned, not the save manager's borrowed +0x18 adapter.
  if (!WriteWord(guest_storage, kAdapterTable) ||
      !WriteWord(guest_storage + 4, guest_storage + 8)) return false;
  storage_ = guest_storage;
  service_ = state.service;
  initialized_ = true;
  return true;
}

SaveUpdate NativeSettingsSave::Enqueue(uint64_t generation, const std::array<uint8_t, 6>& values) {
  if (owner_closed_) return {SaveStatus::Fault, generation, 0, "settings_save_owner_closed"};
  if (!fault_.empty()) return FaultUpdate();
  if (!initialized_) return {SaveStatus::Fault, generation, 0, "settings_save_not_initialized"};
  if (!generation || generation <= last_generation_)
    return {SaveStatus::Fault, generation, 0, "settings_save_stale_generation"};
  if (values[1] > 1 || values[2] > 1 || values[3] < 1 || values[3] > 10 ||
      values[4] < 1 || values[4] > 10 || values[5] < 1 || values[5] > 10)
    return {SaveStatus::Fault, generation, 0, "settings_save_invalid_values"};
  queued_ = Request{generation, values};
  last_generation_ = generation;
  return {SaveStatus::Queued, generation, 0, {}};
}

SaveUpdate NativeSettingsSave::Pump() {
  if (!fault_.empty()) return FaultUpdate();
  if (!initialized_) return {SaveStatus::Fault, 0, 0, "settings_save_not_initialized"};
  if (!inflight_ && !queued_) return {};
  State state;
  if (!ReadState(state)) return FaultUpdate();
  if (!inflight_) {
    // Do not pump another owner's operation or alter its callback. The original
    // request function writes callback/popup fields BEFORE checking its busy flag.
    if (state.operation || state.state || state.callback || state.load_job || state.save_job)
      return {SaveStatus::Busy, queued_->generation, 0, {}};
    if (!ReadRange(state.image, 0xD9EC, "settings_save_image_range") ||
        !Write(storage_ + 8, queued_->values.data(), queued_->values.size())) return FaultUpdate();
    inflight_ = queued_;
    queued_.reset();
    saw_job_ = false;
    uint32_t result = 0;
    if (!state.active) {
      if (!Invoke(SaveCall::WriteRam, storage_, state.payload, 0, result)) return FaultUpdate();
      if (result != 1) { Fail("settings_save_ram_result"); return FaultUpdate(); }
      const auto generation = inflight_->generation;
      inflight_.reset();
      return {SaveStatus::RamOnly, generation, result, {}};
    }
    if (!Invoke(SaveCall::Request, service_, storage_, 0, result)) return FaultUpdate();
    if (result != 1) { Fail("settings_save_request_rejected"); return FaultUpdate(); }
    if (!ReadState(state)) return FaultUpdate();
    if (state.operation != 6 || state.callback != storage_) {
      Fail("settings_save_request_ownership"); return FaultUpdate();
    }
    return {SaveStatus::Requested, inflight_->generation, result, {}};
  }
  if (state.operation != 6 || state.callback != storage_) {
    Fail("settings_save_inflight_ownership"); return FaultUpdate();
  }
  if (state.save_job) saw_job_ = true;
  // Operation16 consumes the callback/payload. The later state-zero job copies
  // the entire serialized image, not just the six edited bytes. Validate that
  // exact source and destination immediately before those native consumers.
  if ((state.state == 16 || (state.save_job && state.save_job_state == 0)) &&
      !ReadRange(state.image, 0xD9EC, "settings_save_image_range")) return FaultUpdate();
  if (state.save_job && state.save_job_state == 0 &&
      !ReadRange(state.save_job, 0xDA30, "settings_save_job_range")) return FaultUpdate();
  uint32_t result = 0;
  if (!Invoke(SaveCall::Pump, service_, 0, 0, result)) return FaultUpdate();
  if (!ReadState(state)) return FaultUpdate();
  if (state.save_job) saw_job_ = true;
  if (result < 2) {
    if (state.operation != 6 || state.callback != storage_) {
      Fail("settings_save_pending_ownership"); return FaultUpdate();
    }
    return {SaveStatus::Pending, inflight_->generation, result, {}};
  }
  if (result > 4 || state.operation || state.state || state.callback || state.load_job || state.save_job) {
    Fail("settings_save_terminal_ownership"); return FaultUpdate();
  }
  const bool disabled = result == 2 && (state.flags & 1) != 0;
  if (disabled) {
    uint32_t ignored = 0;
    // Match 822E6560's context-disable cleanup, but retain the service's exact
    // terminal status rather than confusing next frame's idle result with success.
    if (!Invoke(SaveCall::DisableContext, kSingleton, 0, 0, ignored) ||
        !WriteWord(service_ + 0x1C, 0)) return FaultUpdate();
  }
  const bool complete = result == 2 && !disabled && state.active && state.outcome == 1 && saw_job_;
  const auto generation = inflight_->generation;
  inflight_.reset();
  saw_job_ = false;
  request_may_be_borrowed_ = false;
  return {complete ? SaveStatus::FileComplete : SaveStatus::Abandoned, generation, result, {}};
}

bool NativeSettingsSave::Idle() const {
  return initialized_ && (fault_.empty() || safely_abandoned_) && !inflight_ && !queued_;
}

bool NativeSettingsSave::CanRelease() const {
  if (!initialized_) return false;
  // No adapter callback, read job or write job may still own native work. This
  // check deliberately does not require the failed payload/context read to
  // succeed: those objects do not retain the adapter once the service is idle.
  if (memory_.U32BE(kService) != service_) return false;
  return memory_.U32BE(service_) == 0 && memory_.U32BE(service_ + 0xC) == 0 &&
      memory_.U32BE(service_ + 0x24) == 0 && memory_.U32BE(kSingleton + 4) == 0 &&
      memory_.U32BE(kSingleton + 8) == 0;
}

bool NativeSettingsSave::AbandonQueued() {
  if (fault_.empty() || !CanRelease()) return false;
  queued_.reset();
  inflight_.reset();
  saw_job_ = false;
  request_may_be_borrowed_ = false;
  safely_abandoned_ = true;
  return true;
}

uint64_t NativeSettingsSave::OwnerClosed() {
  owner_closed_ = true;
  uint64_t discarded = queued_ ? queued_->generation : 0;
  queued_.reset();
  if (!request_may_be_borrowed_ && CanRelease()) {
    if (inflight_) discarded = std::max(discarded, inflight_->generation);
    inflight_.reset();
    saw_job_ = false;
    if (!fault_.empty()) safely_abandoned_ = true;
  }
  return discarded;
}

bool NativeSettingsSave::RetryValidation() {
  if (!initialized_ || !inflight_ || !request_may_be_borrowed_ || fault_.empty() || safely_abandoned_)
    return false;
  const auto reason = fault_;
  if (reason != "settings_save_service_identity" && reason != "settings_save_service_state" &&
      reason != "settings_save_singleton_state" && reason != "settings_save_payload_layout" &&
      reason != "settings_save_job_address" && reason != "settings_save_job_state" &&
      reason != "settings_save_image_range" && reason != "settings_save_job_range" &&
      reason != "settings_save_native_context") return false;
  State state;
  if (!ReadState(state) || state.operation != 6 || state.callback != storage_ ||
      !ReadRange(state.image, 0xD9EC, "settings_save_image_range")) return false;
  if (state.save_job && !ReadRange(state.save_job, 0xDA30, "settings_save_job_range")) return false;
  try {
    if (!access_.validate_context || !access_.validate_context()) return false;
  } catch (...) { return false; }
  fault_ = {};
  return true;
}

}  // namespace brawlers::settings
