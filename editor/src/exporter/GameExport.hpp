#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "engine/editor/CommandResult.hpp"

namespace editor {

/** @brief The modes an exported game offers, written as the `modes` of its `game.json`. */
struct ExportModes {
    bool solo = true;
    bool host = true;
    /** Joining a server. */
    bool client = true;
    /** Dedicated server; also ships the `<name>-server` launcher. */
    bool dedicated = true;

    bool operator==(const ExportModes&) const = default;
};

/** @brief The `game.json` names of the chosen modes, in the order solo, host, client, dedicated. */
[[nodiscard]] std::vector<std::string> mode_names(const ExportModes& modes);

/** @brief What the user chooses in the export dialog. */
struct ExportSettings {
    /** File name of the exported game binary; the server launcher is `<name>-server`. */
    std::string name;
    std::filesystem::path output;
    ExportModes modes{};

    bool operator==(const ExportSettings&) const = default;
};

/** @brief One file or folder the export copies. */
struct ExportStep {
    std::filesystem::path from;
    std::filesystem::path to;

    bool operator==(const ExportStep&) const = default;
};

/** @brief A small script the export writes, executable. */
struct ExportLauncher {
    std::filesystem::path to;
    std::string text;

    bool operator==(const ExportLauncher&) const = default;
};

/**
 * @brief Everything an export will do, worked out before anything is written.
 *
 * An export with problems is never run.
 */
struct ExportPlan {
    std::filesystem::path output;
    std::vector<ExportStep> steps;
    /** Each reason the export cannot run, ready to show to the user. */
    std::vector<std::string> problems;
    /** Text of the `README.txt` written next to the binaries: how to run each offered mode. */
    std::string readme;
    /** Modes written into the exported `game.json`; the runtime refuses the others. */
    std::vector<std::string> modes;
    /** `<name>-server`, which runs `<name> --server` with its arguments; only with dedicated. */
    std::optional<ExportLauncher> server_launcher;

    bool operator==(const ExportPlan&) const = default;
};

/** @brief Turns a game name into a binary name: "Space Game" gives "space-game". */
[[nodiscard]] std::string export_name(std::string_view game_name);

/**
 * @brief The settings the dialog opens with: the name from `game.json` (or the folder name),
 * written to `<name>-export` next to the game folder, offering the modes `game.json` lists (every
 * mode when it lists none).
 */
[[nodiscard]] ExportSettings default_export(const std::filesystem::path& game);

/**
 * @brief Lists what exporting @p game with @p settings copies, and why it cannot run.
 *
 * @param runtime Folder holding `engine-runtime`, the `libengine-*` libraries and `plugins/`;
 * usually the editor's own folder.
 * @note Reads the disk but never writes to it.
 */
[[nodiscard]] ExportPlan plan_export(const std::filesystem::path& game,
                                     const std::filesystem::path& runtime,
                                     const ExportSettings& settings);

/**
 * @brief Creates the output folder, copies every step and writes `README.txt`.
 * @pre plan.problems is empty.
 * @return Refused, with the reason, when a copy fails; the output may then be partly written.
 */
[[nodiscard]] engine::editor::CommandResult run_export(const ExportPlan& plan);

} // namespace editor
