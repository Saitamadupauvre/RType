#include "SnapshotView.hpp"

#include <stdexcept>
#include <utility>

#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/Components.hpp"
#include "engine/core/log/Log.hpp"

namespace engine::app {

SnapshotView::SnapshotView(graphics::IRenderer& renderer, std::filesystem::path assets,
                           const core::FieldRegistry& fields)
    : _renderer(renderer), _fields(fields), _draw(renderer, std::move(assets)) {
    _world.register_component<core::Position>();
    _world.register_component<core::Sprite>();
}

std::string SnapshotView::sprite_of(const std::string& type_name) {
    return "sprites/" + type_name + ".png";
}

void SnapshotView::draw(const std::vector<network::EntitySnapshot>& entities,
                        const network::StringTable& strings) {
    mirror(entities, strings);
    _draw.draw(_world);
}

void SnapshotView::mirror(const std::vector<network::EntitySnapshot>& entities,
                          const network::StringTable& strings) {
    for (const auto entity : _mirrored) {
        _world.kill(entity);
    }
    _mirrored.clear();
    for (const auto& snapshot : entities) {
        const std::string* name = strings.name_of(network::StringCategory::type, snapshot.type_id);
        const auto entity = _world.spawn();
        _world.add(entity,
                   core::Sprite{.path = name == nullptr ? "" : sprite_of(*name), .color = {}});
        apply(entity, snapshot);
        _mirrored.push_back(entity);
    }
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
