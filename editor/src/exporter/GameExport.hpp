#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "engine/editor/CommandResult.hpp"

namespace editor {

/** @brief What the user chooses in the export dialog. */
struct ExportSettings {
    /** File name of the exported game binary, which runs every mode. */
    std::string name;
    std::filesystem::path output;

    bool operator==(const ExportSettings&) const = default;
};

/** @brief One file or folder the export copies. */
struct ExportStep {
    std::filesystem::path from;
    std::filesystem::path to;

    bool operator==(const ExportStep&) const = default;
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
    /** Text of the `README.txt` written next to the binaries: how to run every mode. */
    std::string readme;

    bool operator==(const ExportPlan&) const = default;
};

/** @brief Turns a game name into a binary name: "Space Game" gives "space-game". */
[[nodiscard]] std::string export_name(std::string_view game_name);

/**
 * @brief The settings the dialog opens with: the name from `game.json` (or the folder name),
 * written to `<name>-export` next to the game folder.
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
