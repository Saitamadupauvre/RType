#include "render/HitboxDraw.hpp"

#include "engine/core/ecs/ComponentPhysics.hpp"
#include "engine/physics/Components.hpp"

namespace engine::app {

namespace {

constexpr graphics::Color box_color{.r = 0, .g = 255, .b = 120, .a = 255};
constexpr graphics::Color circle_color{.r = 0, .g = 255, .b = 120, .a = 70};

void draw_boxes(graphics::IRenderer& renderer, core::EntityManager& world) {
    if (!world.is_registered<physics::AABBCollider>()) {
        return;
    }
    world.query<core::Position, physics::AABBCollider>().each(
        [&renderer](core::Entity, const core::Position& position,
                    const physics::AABBCollider& collider) {
            const core::Vec2 center = position.pos + collider.offset;
            renderer.draw_rect({.x = center.x - (collider.size.x / 2.F),
                                .y = center.y - (collider.size.y / 2.F),
                                .width = collider.size.x,
                                .height = collider.size.y},
                               box_color, false);
        });
}

void draw_circles(graphics::IRenderer& renderer, core::EntityManager& world) {
    if (!world.is_registered<physics::CircleCollider>()) {
        return;
    }
    world.query<core::Position, physics::CircleCollider>().each(
        [&renderer](core::Entity, const core::Position& position,
                    const physics::CircleCollider& collider) {
            renderer.draw_circle(position.pos + collider.offset, collider.radius, circle_color);
        });
}

} // namespace

void draw_hitboxes(graphics::IRenderer& renderer, core::EntityManager& world) {
    if (!world.is_registered<core::Position>()) {
        return;
    }
    draw_boxes(renderer, world);
    draw_circles(renderer, world);
}

} // namespace engine::app
