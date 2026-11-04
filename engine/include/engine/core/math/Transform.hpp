#pragma once

#include "engine/core/Export.hpp"
#include "engine/core/ecs/Entity.hpp"
#include "engine/core/ecs/EntityManager.hpp"
#include "engine/core/math/Vec2.hpp"

namespace engine::core {

/**
 * @brief Rotation of an entity, in degrees, clockwise on screen since Y points down.
 *
 * @note Optional: an entity without it is not rotated. Colliders ignore it.
 */
struct Rotation {
    float degrees{0.F};

    bool operator==(const Rotation&) const = default;
};

/**
 * @brief Scale factor of an entity on each axis.
 *
 * @note Optional: an entity without it has a scale of (1, 1). A negative factor mirrors the
 * entity on that axis. Colliders ignore it.
 */
struct Scale {
    Vec2 factor{1.F, 1.F};

    bool operator==(const Scale&) const = default;
};

/**
 * @return The rotation of @p entity in degrees, or 0 if it has no Rotation or @p world never
 * registered the type.
 */
[[nodiscard]] ENGINE_CORE_EXPORT float rotation_of(EntityManager& world, Entity entity);

/**
 * @return The scale factor of @p entity, or (1, 1) if it has no Scale or @p world never registered
 * the type.
 */
[[nodiscard]] ENGINE_CORE_EXPORT Vec2 scale_of(EntityManager& world, Entity entity);

} // namespace engine::core
