#include "spatial_audio.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <limits>
#include <thread>
#include <vector>

using namespace brawlers::audio;

namespace {
Scene TwoSources() {
  Scene scene;
  scene.count = 2;
  scene.sources[0] = {11, Cue::Friendly, {-3, 0, 0}, 1};
  scene.sources[1] = {22, Cue::Enemy, {3, 0, 0}, 0.5f};
  return scene;
}
size_t Slot(const Scheduler& scheduler, uint64_t id) {
  for (size_t i = 0; i < kMaxSources; ++i)
    if (scheduler.voices()[i].active && scheduler.voices()[i].source.id == id) return i;
  assert(false);
  return 0;
}
void TestSceneValidationAndDirection() {
  auto scene = TwoSources();
  assert(ValidateScene(scene) == Diagnostic::Count);
  Vec3 direction;
  assert(RelativeDirection(scene, scene.sources[0], direction));
  assert(direction.x == -1 && direction.y == 0 && direction.z == 0);
  scene.listener_ahead = {1, 0, 0};
  assert(RelativeDirection(scene, scene.sources[1], direction));
  assert(direction.x == 0 && direction.z == -1);
  scene.sources[0].position = scene.listener_position;
  assert(ValidateScene(scene) == Diagnostic::Count);
  assert(!RelativeDirection(scene, scene.sources[0], direction));
  scene.count = kMaxSources + 1;
  assert(ValidateScene(scene) == Diagnostic::Capacity);
  scene = TwoSources();
  scene.listener_up = scene.listener_ahead;
  assert(ValidateScene(scene) == Diagnostic::InvalidListener);
  scene = TwoSources();
  scene.sources[1].id = scene.sources[0].id;
  assert(ValidateScene(scene) == Diagnostic::DuplicateIdentity);
  scene = TwoSources();
  scene.sources[0].level = std::numeric_limits<float>::quiet_NaN();
  assert(ValidateScene(scene) == Diagnostic::InvalidSource);
  scene.sources[0].level = 1.01f;
  assert(ValidateScene(scene) == Diagnostic::InvalidSource);
  scene = TwoSources();
  scene.sources[0].position.x = std::numeric_limits<float>::infinity();
  assert(ValidateScene(scene) == Diagnostic::InvalidSource);
}
void TestCadenceAndSurvivors() {
  Scheduler scheduler;
  std::array<Control, 2> controls{};
  auto scene = TwoSources();
  scheduler.Apply(scene, controls);
  const auto slot = Slot(scheduler, 11);
  const auto serial = scheduler.voices()[slot].serial;
  const std::array<float, 5> clip{1, 0.8f, 0.6f, 0.4f, 0.2f};
  std::array<float, 23> actual{};
  scheduler.Fill(slot, clip.data(), clip.size(), actual.data(), 3);
  std::swap(scene.sources[0], scene.sources[1]);
  scene.sources[1].position = {-10, 2, 5};
  scheduler.Apply(scene, controls);
  assert(Slot(scheduler, 11) == slot && scheduler.voices()[slot].serial == serial);
  scheduler.Fill(slot, clip.data(), clip.size(), actual.data() + 3, actual.size() - 3);
  for (size_t i = 0; i < actual.size(); ++i)
    assert(actual[i] == (i % 10 < 5 ? clip[i % 10] : 0));

  // A newly inserted source must not steal a survivor's old slot or phase.
  scene.sources[0] = {33, Cue::Enemy, {1, 2, 3}, 1};
  scheduler.Apply(scene, controls);
  assert(Slot(scheduler, 11) == slot && scheduler.voices()[slot].phase == 3);
  assert(scheduler.voices()[Slot(scheduler, 33)].phase == 0);
  scene.sources[1].cue = Cue::Enemy;
  scheduler.Apply(scene, controls);
  assert(scheduler.voices()[Slot(scheduler, 11)].serial != serial);
  assert(scheduler.voices()[Slot(scheduler, 11)].phase == 0);
}
void TestControlsAndInvalidation() {
  SceneMailbox mailbox;
  const auto closed_token = mailbox.CaptureGeneration();
  assert(!mailbox.Publish(TwoSources(), closed_token));
  mailbox.Open();
  auto token = mailbox.CaptureGeneration();
  assert(mailbox.Publish(TwoSources(), token));
  Snapshot snapshot;
  assert(mailbox.TryRead(snapshot) && snapshot.has_scene);
  Scheduler scheduler;
  scheduler.Apply(snapshot.scene, snapshot.controls);
  const auto friendly = Slot(scheduler, 11), enemy = Slot(scheduler, 22);
  const auto friendly_serial = scheduler.voices()[friendly].serial;
  const auto enemy_serial = scheduler.voices()[enemy].serial;
  // Both writes occur before the renderer sees either change.
  assert(mailbox.Configure(Cue::Friendly, false, 1));
  assert(mailbox.Configure(Cue::Friendly, true, 1));
  assert(mailbox.TryRead(snapshot));
  scheduler.Apply(snapshot.scene, snapshot.controls);
  assert(scheduler.voices()[Slot(scheduler, 11)].serial != friendly_serial);
  assert(scheduler.voices()[enemy].serial == enemy_serial);
  const auto resumed_serial = scheduler.voices()[Slot(scheduler, 11)].serial;
  assert(mailbox.Configure(Cue::Friendly, true, 0.3f));
  assert(mailbox.TryRead(snapshot));
  scheduler.Apply(snapshot.scene, snapshot.controls);
  assert(scheduler.voices()[Slot(scheduler, 11)].serial == resumed_serial);

  mailbox.Invalidate();
  assert(!mailbox.Publish(TwoSources(), token));
  assert(mailbox.TryRead(snapshot) && !snapshot.has_scene);
  token = mailbox.CaptureGeneration();
  assert(mailbox.Publish(TwoSources(), token));
  auto malformed = TwoSources();
  malformed.count = kMaxSources + 1;
  assert(!mailbox.Publish(malformed, token));
  assert(mailbox.TryRead(snapshot) && !snapshot.has_scene);
  assert(mailbox.Diagnostics() & (uint64_t{1} << uint8_t(Diagnostic::Capacity)));
  assert(!mailbox.Configure(Cue::Enemy, true, -1));
  mailbox.Close();
  assert(!mailbox.Publish(TwoSources(), mailbox.CaptureGeneration()));
}
void TestConcurrentSnapshots() {
  SceneMailbox mailbox;
  mailbox.Open();
  std::atomic<bool> done{false};
  std::thread writer([&] {
    for (size_t i = 1; i < 10000; ++i) {
      auto scene = TwoSources();
      scene.listener_position.x = float(i);
      scene.sources[0].position.x = float(i);
      scene.sources[1].position.x = float(i);
      mailbox.Publish(scene, mailbox.CaptureGeneration());
      if (i % 7 == 0) mailbox.Invalidate();
    }
    done.store(true);
  });
  while (!done.load()) {
    Snapshot snapshot;
    if (mailbox.TryRead(snapshot) && snapshot.has_scene) {
      assert(snapshot.scene.listener_position.x == snapshot.scene.sources[0].position.x);
      assert(snapshot.scene.listener_position.x == snapshot.scene.sources[1].position.x);
    }
  }
  writer.join();
}
void CountBlock(void* userdata, float* output) noexcept {
  auto& next = *static_cast<size_t*>(userdata);
  for (size_t i = 0; i < kBlockFrames; ++i) {
    output[2 * i] = float(next);
    output[2 * i + 1] = -float(next++);
  }
}
void TestAdapterAndLimiter() {
  StereoBlockAdapter adapter;
  size_t next = 1;
  std::vector<float> output(2000);
  size_t offset = 0;
  for (size_t count : {1, 317, 19, 511, 152}) {
    adapter.Read(output.data() + offset * 2, count, CountBlock, &next);
    offset += count;
  }
  assert(offset == 1000);
  for (size_t i = 0; i < offset; ++i) {
    assert(output[2 * i] == float(i + 1));
    assert(output[2 * i + 1] == -float(i + 1));
  }
  adapter.Reset();
  next = 42;
  adapter.Read(output.data(), 1, CountBlock, &next);
  assert(output[0] == 42);

  StereoLimiter limiter;
  float quiet[]{0.4f, -0.2f, 0.9f, -0.45f};
  assert(!limiter.Process(quiet, 2));
  assert(quiet[0] == 0.4f && quiet[3] == -0.45f && limiter.gain() == 1);
  float loud[]{4, -2};
  assert(limiter.Process(loud, 1));
  assert(std::abs(loud[0] - 0.95f) < 1e-6 && loud[1] == -loud[0] / 2);
  const auto initial_gain = limiter.gain();
  std::array<float, 4800> recovery{};
  recovery.fill(0.1f);
  assert(!limiter.Process(recovery.data(), recovery.size() / 2));
  const auto expected = 1 - (1 - initial_gain) * std::exp(-1.0);
  assert(std::abs(limiter.gain() - expected) < 0.0001);
  limiter.Reset();
  assert(limiter.gain() == 1);
}
}  // namespace

int main() {
  TestSceneValidationAndDirection();
  TestCadenceAndSurvivors();
  TestControlsAndInvalidation();
  TestConcurrentSnapshots();
  TestAdapterAndLimiter();
}
