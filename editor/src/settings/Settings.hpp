#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "action/EditorState.hpp"
#include "engine/editor/CommandResult.hpp"

namespace editor {

/** @brief One person's editor preferences, read from `settings.json` at startup. */
struct Settings {
    /** Multiplies the screen's DPI scale; from 0.5 to 3. */
    float ui_scale = 1.F;
    /** Multiplies the distance one wheel notch scrolls; from 0.25 to 4. */
    float scroll_speed = 1.F;
    /** What the toolbar starts with; toggling it never writes the file. */
    ViewSettings view;

    bool operator==(const Settings&) const = default;
};

/** @brief Settings read from a file, and every problem found on the way. */
struct LoadedSettings {
    Settings settings;
    /** One line per problem, naming the key; the settings keep their default for it. */
    std::vector<ConsoleLine> problems;
    /** The file the settings come from; empty when no config folder was found. */
    std::filesystem::path file{};
};

/** @brief Reads an environment variable; std::nullopt when it is not set. */
using Environment = std::function<std::optional<std::string>(std::string_view name)>;

/**
 * @brief Where `settings.json` lives: `$XDG_CONFIG_HOME/engine-editor/` (or
 * `~/.config/engine-editor/`) on Linux, `~/Library/Application Support/engine-editor/` on macOS.
 * @return std::nullopt when neither the config folder nor `HOME` is set.
 */
[[nodiscard]] std::optional<std::filesystem::path> settings_path(const Environment& environment);

/**
 * @brief Parses the text of a settings file named @p source in messages.
 * @note Never throws: invalid JSON gives the defaults and one error; an unknown key, a wrong type
 * or an out-of-range value gives one warning and keeps that key's default.
 */
[[nodiscard]] LoadedSettings parse_settings(std::string_view text, std::string_view source);

/**
 * @brief Reads @p file; a missing file gives the defaults and no problem.
 * @note Never throws and never writes the file.
 */
[[nodiscard]] LoadedSettings load_settings(const std::filesystem::path& file);

/**
 * @brief Creates @p file holding an empty JSON object, and its folder, when it does not exist yet.
 * @return Refused, with the reason, when it cannot be created; an existing file is never changed.
 */
[[nodiscard]] engine::editor::CommandResult create_settings_file(const std::filesystem::path& file);

} // namespace editor
