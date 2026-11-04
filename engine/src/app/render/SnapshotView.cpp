#include "render/SnapshotView.hpp"

#include <stdexcept>

#include "engine/core/ecs/ComponentPhysics.hpp"
#include "engine/core/ecs/Components.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/core/math/Transform.hpp"
#include "render/CameraRig.hpp"

namespace engine::app {

SnapshotView::SnapshotView(const core::FieldRegistry& fields) : _fields(fields) {
    _world.register_component<core::Position>();
    _world.register_component<core::Sprite>();
    _world.register_component<core::Layer>();
    _world.register_component<core::Shader>();
    _world.register_component<core::Rotation>();
    _world.register_component<core::Scale>();
}

core::EntityManager& SnapshotView::mirror(const std::vector<network::EntitySnapshot>& entities) {
    for (const auto entity : _mirrored) {
        _world.kill(entity);
    }
    _mirrored.clear();
    for (const auto& snapshot : entities) {
        if (is_camera(snapshot)) {
            continue;
        }
        const auto entity = _world.spawn();
        _world.add(entity, core::Sprite{});
        apply(entity, snapshot);
        _mirrored.push_back(entity);
    }
    return _world;
}

void SnapshotView::apply(core::Entity entity, const network::EntitySnapshot& snapshot) {
    for (const auto& replicated : snapshot.fields) {
        if (!replicated.field.component->is_registered_in(_world)) {
            continue;
        }
        try {
            _fields.set(_world, entity, replicated.field, replicated.value);
        } catch (const std::invalid_argument& error) {
            core::log::warn("client: entity {}: {}", snapshot.entity_id, error.what());
        } catch (const std::out_of_range& error) {
            core::log::warn("client: entity {}: {}", snapshot.entity_id, error.what());
        }
    }
}

} // namespace engine::app
