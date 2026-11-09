#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "engine/core/ecs/Entity.hpp"
#include "engine/core/ecs/FieldRegistry.hpp"
#include "engine/core/math/Vec2.hpp"

namespace engine::script {

/** @brief A field whose value differs from the one its entity type gives. */
struct FieldOverride {
    /**
     * Component and field names joined by a dot, such as "Sprite.color", or "Script." followed by
     * a script field name, such as "Script.health" (see ScriptRuntime::script_fields()).
     */
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
    /** Editable and script fields that differ from the type's defaults, sorted by field name. */
    std::vector<FieldOverride> overrides;

    bool operator==(const Instance&) const = default;
};

/** @brief Something ScriptRuntime::spawn_instances() or ScriptRuntime::read_scene() skipped. */
struct InstanceIssue {
    /**
     * Index of the instance in the span given to spawn_instances(), or in the `instances` array of
     * the file given to read_scene().
     */
    std::size_t instance = 0;
    /** Field of the skipped override, empty when the whole instance was skipped. */
    std::string field;
    std::string message;

    bool operator==(const InstanceIssue&) const = default;
};

/** @brief What ScriptRuntime::preview_instances() built. */
struct Preview {
    /** Entity built for each instance, in the same order; std::nullopt for a skipped instance. */
    std::vector<std::optional<core::Entity>> entities;
    /** Same issues as ScriptRuntime::spawn_instances() would report for these instances. */
    std::vector<InstanceIssue> issues;
};

} // namespace engine::script
