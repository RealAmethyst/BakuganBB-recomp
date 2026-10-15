#pragma once

namespace rex::memory { class Memory; }
namespace brawlers::settings { class HostSettingsRuntime; }
struct PPCContext;

// Install before the guest starts. Runtime owners outlive all guest tasks.
void SetNativeSettings(rex::memory::Memory* memory,
                       brawlers::settings::HostSettingsRuntime* host);

// Called at the existing guest update boundary. Idle host work is skipped;
// detached native save callbacks retain this guest context until released.
void UpdateHostSettings(PPCContext& ctx, unsigned char* base);
