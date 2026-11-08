#pragma once

#include <optional>
#include <string>
#include <vector>

#include "engine/core/ecs/FieldRegistry.hpp"
#include "engine/editor/EditorSession.hpp"
#include "engine/editor/SceneDocument.hpp"

namespace editor {

/** @brief One field of the selected instance, as the scene gives it: an override or the prefab's value. */
struct InspectorField {
    /** "Component.field" or "Script.<name>", as SetField expects it. */
    std::string key;
    engine::core::FieldKind kind = engine::core::FieldKind::Float;
    engine::core::FieldValue value;
    bool overridden = false;

    bool operator==(const InspectorField&) const = default;
};

/** @brief What the inspector shows for the selected instance. */
struct InspectorModel {
    engine::editor::InstanceId id;
    std::vector<InspectorField> fields;
    std::vector<InspectorField> script_fields;

    bool operator==(const InspectorModel&) const = default;
};

/**
 * @brief Lists the editable fields of the selected instance: registry fields of the components
 * its entity owns, in registration order, then its script fields. `Position.pos` is left out:
 * it is the instance position, changed with Move.
 * @return std::nullopt without a selection.
 */
[[nodiscard]] std::optional<InspectorModel> inspect_selection(engine::editor::EditorSession& session);

} // namespace editor
