#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "engine/core/FieldRegistry.hpp"
#include "engine/core/Vec2.hpp"

namespace engine::script {

/** @brief A component field whose value differs from the one its entity type gives. */
struct FieldOverride {
    /** Component and field names joined by a dot, such as "Sprite.color". */
    std::string field;
    core::FieldValue value;

    bool operator==(const FieldOverride&) const = default;
};

/**
 * @brief One entity of a world, stored as data: its type, its position and what it changes.
 *
 * Produced by ScriptRuntime::capture_instances() and turned back into an entity by
 * ScriptRuntime::spawn_instances(). Holds no file format: scene files and the editor build on it.
 */
struct Instance {
    /** Label given with ScriptRuntime::set_entity_name(), empty when the entity has none. */
    std::string name;
    /** Declared entity type. */
    std::string type;
    core::Vec2 position;
    /** Editable fields that differ from the type's defaults, sorted by field name. */
    std::vector<FieldOverride> overrides;

    bool operator==(const Instance&) const = default;
};

/** @brief Something ScriptRuntime::spawn_instances() skipped. */
struct InstanceIssue {
    /** Index of the instance in the span given to spawn_instances(). */
    std::size_t instance = 0;
    /** Field of the skipped override, empty when the whole instance was skipped. */
    std::string field;
    std::string message;

    bool operator==(const InstanceIssue&) const = default;
};

} // namespace engine::script
