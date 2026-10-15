#include "settings_catalog.h"

#include <stdexcept>

namespace brawlers::settings {

std::optional<Definition> BuildDefinition(const Catalog& c) {
  try {
    Definition result;
    result.title = c.title;
    result.root = 1;
    if (result.title.empty()) return std::nullopt;
    const auto text = [&](const char* key) -> const std::string& { return c.text.at(key); };
    const auto page = [&](Id id, const std::string& label) {
      return Page{id, label, text("hints"), {}};
    };
    const auto link = [&](Id id, Id target, const char* key) {
      Row row;
      row.id = id; row.kind = RowKind::Page; row.page = target; row.label = text(key);
      return row;
    };
    const auto reset = [&](Id id, std::vector<Id> targets) {
      Row row;
      row.id = id; row.kind = RowKind::Reset; row.label = text("restore_defaults");
      row.confirmation = text("restore_question"); row.yes = text("yes"); row.no = text("no");
      row.reset_rows = std::move(targets);
      return row;
    };
    const std::vector<Value> toggle{{0, text("off")}, {1, text("on")}};
    const auto native = [&](size_t index, Id id) {
      Row row;
      row.id = id; row.label = c.native_labels.at(index);
      row.value = c.native_values.at(index + 1);
      row.default_value = index < 2 ? 1 : 10;
      row.kind = index < 2 ? RowKind::Toggle : RowKind::Range;
      row.values = index < 2 ? toggle : c.native_volumes;
      row.minimum = 1; row.maximum = 10;
      return row;
    };
    const auto host = [&](Id id, const char* key, RowKind kind,
                          const std::vector<Value>& choices, const char* description = nullptr) {
      Row row;
      row.id = id; row.label = text(key); row.kind = kind; row.values = choices;
      row.value = c.host_values.at(id); row.default_value = c.host_defaults.at(id);
      if (description) row.description = text(description);
      if (kind == RowKind::Range) { row.minimum = 0; row.maximum = 100; row.step = 10; }
      return row;
    };

    auto root = page(1, "");
    root.rows = {link(1, 2, "display"), link(2, 3, "audio"), link(3, 4, "controls"),
                 link(4, 5, "accessibility"), link(5, 6, "advanced")};
    auto display = page(2, text("display"));
    display.rows = {
        host(Fullscreen, "fullscreen", RowKind::Toggle, toggle),
        host(WindowSize, "window_size", RowKind::Choice, c.window_sizes, "window_size_description"),
        host(VSync, "vsync", RowKind::Toggle, toggle, "vsync_description"),
        reset(401, {Fullscreen, WindowSize, VSync})};
    auto audio = page(3, text("audio"));
    audio.rows = {native(2, Music), native(3, Sfx), native(4, Voice),
                  reset(402, {Music, Sfx, Voice})};
    auto controls = page(4, text("controls"));
    controls.rows = {native(0, ThrowingPractice), native(1, Vibration),
                     reset(403, {ThrowingPractice, Vibration})};
    auto accessibility = page(5, text("accessibility"));
    accessibility.rows = {link(6, 7, "audio"),
        reset(404, {FriendlyEnabled, FriendlyVolume, EnemyEnabled, EnemyVolume})};
    auto advanced = page(6, text("advanced"));
    advanced.rows = {
        host(RenderScale, "render_scale", RowKind::Choice, c.render_scales, "render_scale_description"),
        reset(405, {RenderScale})};
    auto cues = page(7, text("accessibility_audio"));
    cues.rows = {
        host(FriendlyEnabled, "friendly_enabled", RowKind::Toggle, toggle),
        host(FriendlyVolume, "friendly_volume", RowKind::Range, c.cue_volumes),
        host(EnemyEnabled, "enemy_enabled", RowKind::Toggle, toggle),
        host(EnemyVolume, "enemy_volume", RowKind::Range, c.cue_volumes),
        reset(406, {FriendlyEnabled, FriendlyVolume, EnemyEnabled, EnemyVolume})};
    if (const auto unavailable = c.text.find("cue_unavailable"); unavailable != c.text.end())
      for (auto& row : cues.rows)
        if (row.kind != RowKind::Reset) row.description = unavailable->second;
    result.pages = {std::move(root), std::move(display), std::move(audio),
                    std::move(controls), std::move(accessibility), std::move(advanced), std::move(cues)};
    Menu validation;
    if (!validation.Configure(result)) return std::nullopt;
    return result;
  } catch (const std::out_of_range&) {
    return std::nullopt;
  }
}

}  // namespace brawlers::settings
