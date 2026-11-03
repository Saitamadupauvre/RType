#include "WorldDraw.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/Components.hpp"
#include "engine/core/Transform.hpp"

namespace engine::app {

namespace {

constexpr float square_size = 64.F;

graphics::Color to_color(core::Rgba color) {
    return {.r = color.r, .g = color.g, .b = color.b, .a = color.a};
}

core::Vec2 drawn_size(core::Vec2 size, core::Vec2 scale) {
    return {size.x * std::abs(scale.x), size.y * std::abs(scale.y)};
}

float mirrored(float length, float factor) { return std::signbit(factor) ? -length : length; }

std::int32_t z_index_of(core::EntityManager& world, core::Entity entity) {
    if (!world.is_registered<core::Layer>() || !world.has<core::Layer>(entity)) {
        return 0;
    }
    return world.get<core::Layer>(entity).z_index;
}

} // namespace

WorldDraw::WorldDraw(graphics::IRenderer& renderer, TextureLibrary& textures,
                     ShaderLibrary& shaders)
    : _renderer(renderer), _textures(textures), _shaders(shaders) {}

void WorldDraw::draw(core::EntityManager& world, float seconds) {
    draw({std::ref(world)}, seconds);
}

void WorldDraw::draw(std::initializer_list<std::reference_wrapper<core::EntityManager>> worlds,
                     float seconds) {
    _queue.clear();
    for (core::EntityManager& world : worlds) {
        queue(world);
    }
    std::ranges::stable_sort(_queue, std::less{}, &Queued::z_index);
    for (const Queued& item : _queue) {
        draw_queued(item, seconds);
    }
}

void WorldDraw::queue(core::EntityManager& world) {
    world.query<core::Position, core::Sprite>().each([&](core::Entity entity, const core::Position&,
                                                         const core::Sprite&) {
        _queue.push_back({.z_index = z_index_of(world, entity), .world = &world, .entity = entity});
    });
}

void WorldDraw::draw_queued(const Queued& item, float seconds) {
    core::EntityManager& world = *item.world;
    const core::Vec2 position = world.get<core::Position>(item.entity).pos;
    const core::Sprite& sprite = world.get<core::Sprite>(item.entity);
    const graphics::ShaderId shader = shader_of(world, item.entity);
    if (shader == graphics::ShaderId{}) {
        draw_entity(world, item.entity, position, sprite);
        return;
    }
    begin_shader(shader, seconds);
    draw_entity(world, item.entity, position, sprite);
    _renderer.end_shader();
}

void WorldDraw::draw_entity(core::EntityManager& world, core::Entity entity, core::Vec2 position,
                            const core::Sprite& sprite) {
    const core::Vec2 scale = core::scale_of(world, entity);
    if (sprite.path.empty()) {
        draw_square(position, scale, sprite.color);
        return;
    }
    draw_texture(position, scale, core::rotation_of(world, entity), sprite);
}

graphics::ShaderId WorldDraw::shader_of(core::EntityManager& world, core::Entity entity) {
    if (!world.is_registered<core::Shader>() || !world.has<core::Shader>(entity)) {
        return {};
    }
    const std::string& path = world.get<core::Shader>(entity).path;
    return path.empty() ? graphics::ShaderId{} : _shaders.get(path);
}

void WorldDraw::begin_shader(graphics::ShaderId shader, float seconds) {
    _renderer.set_uniform(shader, "time", seconds);
    _renderer.set_uniform(shader, "resolution", _renderer.virtual_size());
    _renderer.begin_shader(shader);
}

void WorldDraw::draw_square(core::Vec2 position, core::Vec2 scale, core::Rgba color) {
    const core::Vec2 size = drawn_size({square_size, square_size}, scale);
    _renderer.draw_rect({.x = position.x, .y = position.y, .width = size.x, .height = size.y},
                        to_color(color));
}

void WorldDraw::draw_texture(core::Vec2 position, core::Vec2 scale, float rotation,
                             const core::Sprite& sprite) {
    const graphics::TextureId id = _textures.get(sprite.path);
    const core::Vec2 texture_size = _renderer.texture_size(id);
    const core::Vec2 size = drawn_size(texture_size, scale);
    const core::Vec2 center = size / 2.F;
    _renderer.draw_sprite({.texture = id,
                           .source = {.width = mirrored(texture_size.x, scale.x),
                                      .height = mirrored(texture_size.y, scale.y)},
                           .dest = {.x = position.x + center.x,
                                    .y = position.y + center.y,
                                    .width = size.x,
                                    .height = size.y},
                           .origin = center,
                           .rotation = rotation,
                           .tint = to_color(sprite.color)});
}

} // namespace engine::app
