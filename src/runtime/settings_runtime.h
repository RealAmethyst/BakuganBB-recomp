#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../core/settings_catalog.h"

namespace rex {
class Runtime;
namespace ui { class WindowedAppContext; class Window; class Presenter; }
}

namespace brawlers::settings {

enum class HostEventKind { Committed, Rejected, Applied, ApplyFailed, Saved, SaveFailed };
struct HostEvent {
  uint64_t generation = 0;
  HostEventKind kind = HostEventKind::Rejected;
  // Zero denotes the entire command (commit/save); display outcomes name a row.
  Id row = 0;
  bool restart_required = false;
  // Diagnostic detail, not localized speech. Present central resource messages.
  std::string error;
};

struct HostCatalogSnapshot {
  Catalog catalog;
  uint64_t revision = 0;
};

// Configure/Update/Shutdown and configured destruction belong to the UI thread. Submit, Poll and
// ReadCatalog may run on the guest thread. SDK owners must outlive Shutdown.
// Committed means desired values and an immutable save job were accepted;
// it does not promise OS application or disk persistence. Only Committed or
// Rejected resolves the menu command. Other events report later outcomes.
class HostSettingsRuntime {
 public:
  HostSettingsRuntime();
  ~HostSettingsRuntime();
  HostSettingsRuntime(const HostSettingsRuntime&) = delete;
  HostSettingsRuntime& operator=(const HostSettingsRuntime&) = delete;
  bool Configure(rex::Runtime* runtime, rex::ui::WindowedAppContext* context,
                 rex::ui::Window* window, rex::ui::Presenter* presenter,
                 const std::filesystem::path& config_path,
                 const std::filesystem::path& installation, std::string* error = nullptr);
  std::optional<Catalog> ReadCatalog() const;
  // The cheap change token performs no registry reads. Zero means unavailable.
  // Nonzero revisions increase on desired commits and external flag changes.
  uint64_t CatalogRevision() const;
  // Copies a consistent revision, or returns empty if unavailable or changing
  // during all bounded attempts. Retry later; retain the previous menu view.
  std::optional<HostCatalogSnapshot> ReadCatalogSnapshot() const;
  bool Submit(const Command& command);
  // Continue polling pending work after closing the menu. Poll schedules a
  // coalesced UI check; it never accesses a window from the calling thread.
  std::optional<HostEvent> Poll();
  bool HasPendingWork() const;
  void UpdateFromUIThread();
  void Shutdown();

 private:
  struct State;
  std::shared_ptr<State> state_;
};

}  // namespace brawlers::settings
