#pragma once

#include "guest_ui_memory.h"

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>

namespace brawlers::settings {

// The integrator dispatches these verified original guest functions. Arguments
// are guest r3/r4/r5; the output is the guest r3 result. No host addresses cross
// this interface. Invoke on the guest thread, outside observer/runtime locks.
enum class SaveCall : uint32_t {
  Request = 0x821C0880,
  Pump = 0x821C1DA8,
  WriteRam = 0x822E5D38,
  DisableContext = 0x821BC008
};
struct SaveAccess {
  ui::ReadBytes read;
  std::function<bool(uint32_t, const void*, size_t)> write;
  std::function<bool(SaveCall, uint32_t, uint32_t, uint32_t, uint32_t&)> call;
  // Required before every native invocation. The owner validates the active
  // native Options task/session and all runtime/input/common/device objects
  // consumed by these functions. Once Request has borrowed the owned adapter,
  // these no longer depend on the Options task: an orphan's validator must
  // retain native save/common/input/render/device lifetime rather than require
  // the removed task. Structural checks here are not type checks.
  std::function<bool()> validate_context;
};
enum class SaveStatus { Idle, Queued, Busy, Requested, Pending, RamOnly, FileComplete, Abandoned, Fault };
struct SaveUpdate {
  SaveStatus status = SaveStatus::Idle;
  uint64_t generation = 0;
  uint32_t native_result = 0;
  std::string_view reason;
};

// Single guest-thread owner. Supported lifetime: one open native Options task;
// native retry dialogs own intentional slot/device changes during that session.
// Do not reject those changes against the original slot or backing descriptor:
// native retry temporarily swaps descriptors and may select slot zero.
//
// Init borrows a dedicated, aligned 16-byte guest allocation from the caller.
// It installs the original adapter table and a pointer to its own six bytes.
// That allocation must remain alive through the ENTIRE native service request,
// including failure dialogs and retries, not merely through job completion.
// A fault does not grant permission to free a possibly retained allocation.
// There is no background worker, destructor cleanup or native-call wrapper here.
class NativeSettingsSave {
 public:
  explicit NativeSettingsSave(SaveAccess access);
  bool Init(uint32_t guest_storage);
  SaveUpdate Enqueue(uint64_t generation, const std::array<uint8_t, 6>& values);
  SaveUpdate Pump();
  // This adapter has no queued/borrowed request. The owner must still verify
  // that the shared service is idle before invoking the original close save.
  bool Idle() const;
  // Independent current native ownership proof, including after a fault. This
  // proves only allocation lifetime, not readiness for another native UI call.
  bool CanRelease() const;
  // Permanently reject new edits after the Options owner disappears. Discards
  // queued work, but preserves a borrowed snapshot for native retry/completion.
  // Returns the highest discarded unsaved generation, or zero. A pre-request
  // fault's snapshot is discarded only with CanRelease proof; this may be
  // called again when that proof becomes readable. Does not invoke native code.
  uint64_t OwnerClosed();
  // Explicitly abandon failed, unsaved queued work only when CanRelease proves
  // native ownership has ended. Keeps the diagnostic; Idle then permits exit.
  // Never abandons an active service, and never drops normally queued work.
  bool AbandonQueued();
  // Retry only transient read/range/context faults on the SAME borrowed native
  // operation, after ownership, snapshot, ranges and context validate again.
  // Native-call failures/exceptions, writes and corrupted snapshots stay fatal.
  bool RetryValidation();
  std::string_view fault() const { return fault_; }

 private:
  struct Request {
    uint64_t generation = 0;
    std::array<uint8_t, 6> values{};
  };
  struct State {
    uint32_t service = 0, state = 0, operation = 0, callback = 0, flags = 0;
    uint32_t descriptor = 0, image = 0, payload = 0, load_job = 0, save_job = 0;
    uint8_t active = 0, outcome = 0;
    uint32_t save_job_state = 0;
  };
  bool ReadRange(uint32_t address, size_t size, std::string_view reason);
  bool ReadState(State& state);
  bool Invoke(SaveCall call, uint32_t r3, uint32_t r4, uint32_t r5, uint32_t& result);
  bool Write(uint32_t address, const void* bytes, size_t size);
  bool WriteWord(uint32_t address, uint32_t value);
  bool Fail(std::string_view reason);
  SaveUpdate FaultUpdate() const;

  SaveAccess access_;
  ui::GuestUiMemory memory_;
  uint32_t storage_ = 0;
  uint32_t service_ = 0;
  uint64_t last_generation_ = 0;
  std::optional<Request> queued_;
  std::optional<Request> inflight_;
  std::string_view fault_;
  bool saw_job_ = false;
  bool initialized_ = false;
  bool request_may_be_borrowed_ = false;
  bool safely_abandoned_ = false;
  bool owner_closed_ = false;
};

}  // namespace brawlers::settings
