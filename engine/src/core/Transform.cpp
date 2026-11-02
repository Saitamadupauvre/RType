#include "engine/core/Transform.hpp"

namespace engine::core {

namespace {

template <typename Component> bool owns(const EntityManager& world, Entity entity) {
    return world.is_registered<Component>() && world.has<Component>(entity);
}

} // namespace

float rotation_of(EntityManager& world, Entity entity) {
    return owns<Rotation>(world, entity) ? world.get<Rotation>(entity).degrees : Rotation{}.degrees;
}

Vec2 scale_of(EntityManager& world, Entity entity) {
    return owns<Scale>(world, entity) ? world.get<Scale>(entity).factor : Scale{}.factor;
}

} // namespace engine::core
