#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

namespace editor {

/** @brief A place in a script, as Lua names it in an error: `enemies/bydo.lua:12:`. */
struct ScriptLocation {
    /** Path relative to the game's `scripts/` folder. */
    std::filesystem::path file;
    int line = 0;

    bool operator==(const ScriptLocation&) const = default;
};

/**
 * @brief Finds the first `<path>.lua:<line>:` in @p text.
 *
 * The path is made of letters, digits, `_`, `-`, `.` and `/`, is relative and never climbs out
 * with `..`; anything else gives std::nullopt.
 */
[[nodiscard]] std::optional<ScriptLocation> script_location(std::string_view text);

} // namespace editor
