#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "engine/script/Export.hpp"
#include "engine/script/Instance.hpp"

namespace engine::script {

/**
 * @brief The content of one scene file: the instances a game spawns when the scene starts.
 *
 * Read with ScriptRuntime::read_scene() and written with write_scene(). The file format is
 * described in docs/game-folder.md.
 */
struct Scene {
    /** Instances whose type is a declared entity, in file order, overrides sorted by field. */
    std::vector<Instance> instances;
    /**
     * Instances whose type no loaded script declares, each kept as the JSON text it had in the
     * file, so that saving a scene while a script is missing never loses them.
     */
    std::vector<std::string> unknown_instances;

    bool operator==(const Scene&) const = default;
};

/** @brief What ScriptRuntime::read_scene() got from a scene file. */
struct SceneLoad {
    /** The instances read, empty when @ref error is set. */
    Scene scene;
    /**
     * Instances and overrides that were skipped, each with the index of its instance in the
     * file's `instances` array.
     */
    std::vector<InstanceIssue> issues;
    /**
     * Why the whole file was rejected (missing, not valid JSON, unsupported version, wrong
     * shape), empty when it was read.
     */
    std::string error;

    /** @return true when the file was read, even if some of its instances were skipped. */
    [[nodiscard]] bool ok() const noexcept { return error.empty(); }
};

/**
 * @brief Writes a scene file.
 *
 * Writes the instances, then the unknown instances unchanged, as indented JSON. Floats are written
 * with the fewest digits that read back to the same value. The text first goes to `<file>.tmp`,
 * which then replaces @p file, so a failed write never leaves a truncated scene. Missing parent
 * folders are created.
 *
 * @param file Destination, usually `<game>/scenes/<name>.json`.
 * @param scene Scene to write.
 * @return true if the file was written; false, leaving @p file unchanged, if it cannot be written
 * or an unknown instance is not a JSON object. The error is logged.
 * @note Never throws. Needs no runtime: the kind of every value is known from the value itself.
 */
ENGINE_SCRIPT_EXPORT bool write_scene(const std::filesystem::path& file,
                                      const Scene& scene) noexcept;

} // namespace engine::script
