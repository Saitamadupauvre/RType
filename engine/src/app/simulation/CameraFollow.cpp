#include "simulation/CameraFollow.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "engine/core/ecs/ComponentPhysics.hpp"
#include "engine/core/ecs/Components.hpp"
#include "engine/core/math/Camera.hpp"

namespace engine::app {

namespace {

std::optional<core::Vec2> position_of_first_tagged(core::EntityManager& world,
                                                   const std::string& tag, core::Entity camera) {
    std::optional<std::pair<std::uint32_t, core::Vec2>> found;
    world.query<core::Position, core::Tags>().each(
        [&](core::Entity entity, const core::Position& position, const core::Tags& tags) {
            if (entity == camera || !tags.has(tag)) {
                return;
            }
            if (!found || entity.index() < found->first) {
                found.emplace(entity.index(), position.pos);
            }
        });
    if (!found) {
        return std::nullopt;
    }
    return found->second;
}

} // namespace

void follow_cameras(core::EntityManager& world) {
    if (!world.is_registered<core::Camera>() || !world.is_registered<core::Tags>()) {
        return;
    }
    std::vector<std::pair<core::Entity, core::Vec2>> moves;
    world.query<core::Position, core::Camera>().each(
        [&](core::Entity entity, const core::Position&, const core::Camera& camera) {
            if (camera.follow.empty()) {
                return;
            }
            if (const auto target = position_of_first_tagged(world, camera.follow, entity)) {
                moves.emplace_back(entity, *target);
            }
        });
    for (const auto& [entity, target] : moves) {
        world.get<core::Position>(entity).pos = target;
    }
}

} // namespace engine::app
