#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "action/Action.hpp"
#include "action/InspectorModel.hpp"
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

    bool operator==(const ConsoleLine&) const = default;
};

/** @brief The open export dialog: what the user chose and what exporting it would do. */
struct ExportDialog {
    ExportSettings settings;
    ExportPlan plan;

    bool operator==(const ExportDialog&) const = default;
};

/**
 * @brief Everything the UI shows besides the session, owned and changed only by Editor.
 *
 * `prefabs` and `scenes` cache EditorSession::prefabs() and EditorSession::scenes(), which scan
 * the disk, and are refreshed after every command. `inspector` caches inspect_selection() and is
 * refreshed after every action.
 */
struct EditorState {
    Mode mode = Mode::editing;
    Tool tool = Tool::select;
    BottomTab bottom_tab = BottomTab::console;
    ViewSettings view;
    std::vector<ConsoleLine> console;
    std::vector<std::string> prefabs;
    std::vector<std::string> scenes;
    std::string prefab_filter;
    std::optional<InspectorModel> inspector;
    std::optional<ExportDialog> export_dialog;
    /** The user's settings file; empty when no config folder was found. */
    std::filesystem::path settings_file;
};

} // namespace editor
