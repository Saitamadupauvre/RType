#pragma once

#include <compare>
#include <cstdint>
#include <string>
#include <vector>

#include "engine/editor/Export.hpp"
#include "engine/script/Instance.hpp"
#include "engine/script/Scene.hpp"

namespace engine::editor {

/**
 * @brief Identifies an instance of the open scene for as long as the scene stays open.
 *
 * Given by EditorSession when an instance is read or placed, kept through undo and redo, never
 * reused within a session and never saved: reopening a scene gives new ids.
 */
struct InstanceId {
    std::uint64_t value = 0;

    auto operator<=>(const InstanceId&) const = default;
};

/** @brief One instance of the scene document with its id. */
struct DocumentInstance {
    InstanceId id;
    script::Instance instance;

    bool operator==(const DocumentInstance&) const = default;
};

/**
 * @brief The scene being edited: the source of truth of the editor.
 *
 * Instances keep their file order. Instances of types the scripts do not declare are kept as
 * read in @ref unknown_instances and written back on save.
 */
struct ENGINE_EDITOR_EXPORT SceneDocument {
    std::vector<DocumentInstance> instances;
    std::vector<std::string> unknown_instances;

    /** @return The instance with @p id, or nullptr. */
    [[nodiscard]] const DocumentInstance* find(InstanceId id) const noexcept;
    /** @copydoc find(InstanceId) const */
    [[nodiscard]] DocumentInstance* find(InstanceId id) noexcept;

    /** @return The document as a scene, ready for script::write_scene(). */
    [[nodiscard]] script::Scene to_scene() const;

    bool operator==(const SceneDocument&) const = default;
};

} // namespace engine::editor
