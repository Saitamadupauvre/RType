#include "engine/core/Registry.hpp"

#include <string>

namespace engine::core {

ComponentNotRegistered::ComponentNotRegistered(std::string_view type_name)
    : std::logic_error("component type not registered: " + std::string(type_name)) {}

ComponentAlreadyRegistered::ComponentAlreadyRegistered(std::string_view type_name)
    : std::logic_error("component type already registered: " + std::string(type_name)) {}

Entity Registry::spawn() { return _entities.create(); }

bool Registry::kill(Entity entity) {
    if (!_entities.destroy(entity)) {
        return false;
    }
    for (const auto& pool : _pools) {
        if (pool) {
            pool->erase(entity);
        }
    }
    return true;
}

bool Registry::alive(Entity entity) const noexcept { return _entities.alive(entity); }

} // namespace engine::core
