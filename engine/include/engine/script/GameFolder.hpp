#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "engine/script/Export.hpp"

namespace engine::script {

/** @brief What `game.json` says about a game folder. */
struct GameProject {
    /** Name of the game, or std::nullopt when `game.json` names none or it is not a string. */
    std::optional<std::string> name;
    /** Scene spawned when the game starts, or std::nullopt when `game.json` names none. */
    std::optional<std::string> start_scene;
    /**
     * Modes the game may run in, among `solo`, `host`, `client` and `dedicated`, in file order
     * without duplicates; std::nullopt when `game.json` lists none, meaning every mode.
     */
    std::optional<std::vector<std::string>> modes;

    bool operator==(const GameProject&) const = default;
};

/**
 * @brief Tells whether @p name can name a scene: 1 to 63 letters, digits, `_` or `-`.
 *
 * A valid name never leaves the `scenes/` folder of a game.
 */
[[nodiscard]] ENGINE_SCRIPT_EXPORT bool is_scene_name(std::string_view name) noexcept;

/**
 * @brief Tells whether @p name is a mode a game folder can list: `solo`, `host`, `client` or
 * `dedicated`.
 */
[[nodiscard]] ENGINE_SCRIPT_EXPORT bool is_mode_name(std::string_view name) noexcept;

/**
 * @brief Path of the file of scene @p name: `<game>/scenes/<name>.json`.
 * @pre is_scene_name(name).
 */
[[nodiscard]] ENGINE_SCRIPT_EXPORT std::filesystem::path
scene_file(const std::filesystem::path& game, std::string_view name);

/** @brief Path of the scripts of a game: `<game>/scripts`. */
[[nodiscard]] ENGINE_SCRIPT_EXPORT std::filesystem::path
scripts_folder(const std::filesystem::path& game);

/**
 * @brief Reads `<game>/game.json`.
 *
 * @return An empty project when the file does not exist; std::nullopt, after logging the error,
 * when it cannot be read, is not a JSON object, has a `start_scene` that is not a scene name, or
 * has `modes` that are not a non-empty array of mode names (see is_mode_name()).
 * @note Never throws for invalid content.
 */
[[nodiscard]] ENGINE_SCRIPT_EXPORT std::optional<GameProject>
read_project(const std::filesystem::path& game);

/**
 * @brief Sets or removes the `start_scene` of `<game>/game.json`, keeping its other keys.
 *
 * Creates the file when it does not exist. The new text replaces the old file only once it is
 * fully written.
 *
 * @param start_scene New start scene, or std::nullopt to remove the key.
 * @return false, after logging the error, when the file cannot be read or written, is not a JSON
 * object, or @p start_scene is not a scene name.
 * @note Never throws.
 */
ENGINE_SCRIPT_EXPORT bool write_start_scene(const std::filesystem::path& game,
                                            const std::optional<std::string>& start_scene) noexcept;

/**
 * @brief Sets or removes the `modes` of `<game>/game.json`, keeping its other keys.
 *
 * Creates the file when it does not exist. The new text replaces the old file only once it is
 * fully written.
 *
 * @param modes New modes, or std::nullopt to remove the key, meaning every mode.
 * @return false, after logging the error, when the file cannot be read or written, is not a JSON
 * object, or @p modes is empty or holds a name that is not a mode name.
 * @note Never throws.
 */
ENGINE_SCRIPT_EXPORT bool
write_modes(const std::filesystem::path& game,
            const std::optional<std::vector<std::string>>& modes) noexcept;

/**
 * @brief Lists the scenes of a game: the `<name>.json` files of `<game>/scenes` whose name is a
 * scene name, in ascending order.
 *
 * @return The names, without extension; empty when the folder does not exist.
 * @note Never throws.
 */
[[nodiscard]] ENGINE_SCRIPT_EXPORT std::vector<std::string>
list_scenes(const std::filesystem::path& game) noexcept;

} // namespace engine::script
