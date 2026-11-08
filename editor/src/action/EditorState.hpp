#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "action/Action.hpp"
#include "action/InspectorModel.hpp"
#include "assets/AssetList.hpp"
#include "exporter/GameExport.hpp"

namespace editor {

/** @brief Whether the open scene is being edited or played. */
enum class Mode : std::uint8_t { editing, playing, paused };

/** @brief How the viewport shows the scene; never saved in the scene. */
struct ViewSettings {
    bool snap = true;
    float grid_size = 16.F;
    bool show_hitboxes = false;
    bool show_names = true;

    bool operator==(const ViewSettings&) const = default;
};

enum class Severity : std::uint8_t { info, warning, error };

/** @brief A line of the editor console, usually the message of a refused command. */
struct ConsoleLine {
    Severity severity = Severity::info;
    std::string text;
    /** @brief When the line was written, as "HH:MM:SS"; empty when no clock is attached. */
    std::string time{};
    /** @brief The script the line names (`player.lua:12:`) when it exists; clicking opens it. */
    std::filesystem::path script{};

    bool operator==(const ConsoleLine&) const = default;
};

/** @brief The open export dialog: what the user chose and what exporting it would do. */
struct ExportDialog {
    ExportSettings settings;
    ExportPlan plan;

    bool operator==(const ExportDialog&) const = default;
};

/** @brief The open name dialog: what it creates and the name typed so far. */
struct NameDialog {
    NewItem item = NewItem::prefab;
    std::string name;

    bool operator==(const NameDialog&) const = default;
};

/** @brief The console keeps this many lines and drops the oldest ones beyond it. */
inline constexpr std::size_t max_console_lines = 500;

/** @brief The last refused action, so the UI can flash it where the user is looking. */
struct Refusal {
    /** Counts refusals; a new value means a new refusal, even with the same text. */
    std::uint64_t count = 0;
    std::string text;

    bool operator==(const Refusal&) const = default;
};

/**
 * @brief Everything the UI shows besides the session, owned and changed only by Editor.
 *
 * `prefabs`, `scenes` and `assets` cache EditorSession::prefabs(), EditorSession::scenes() and
 * list_assets(), which scan the disk, and are refreshed after every command. `inspector` caches
 * inspect_selection() and is refreshed after every action.
 */
struct EditorState {
    Mode mode = Mode::editing;
    Tool tool = Tool::select;
    BottomTab bottom_tab = BottomTab::console;
    ViewSettings view;
    /** The last max_console_lines lines, oldest first. */
    std::vector<ConsoleLine> console;
    Refusal refusal;
    std::vector<std::string> prefabs;
    std::vector<std::string> scenes;
    /** The files of `<game>/assets/`, refreshed with `prefabs` and `scenes`. */
    std::vector<AssetEntry> assets;
    std::string prefab_filter;
    std::optional<InspectorModel> inspector;
    std::optional<ExportDialog> export_dialog;
    std::optional<NameDialog> name_dialog;
    /** The user's settings file; empty when no config folder was found. */
    std::filesystem::path settings_file;
    /** `ui.scale` of the settings: multiplies the screen's DPI scale. */
    float ui_scale = 1.F;
    /** `ui.scroll_speed` of the settings: multiplies the wheel step. */
    float scroll_speed = 1.F;
};

} // namespace editor
