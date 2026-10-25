#include "engine/core/EntityPool.hpp"

namespace engine::core {

Entity EntityPool::create() {
    if (_free_indices.empty()) {
        const auto index = static_cast<std::uint32_t>(_generations.size());
        _generations.push_back(0);
        _alive.push_back(true);
        return Entity{index, 0};
    }
    const std::uint32_t index = _free_indices.back();
    _free_indices.pop_back();
    _alive[index] = true;
    return Entity{index, _generations[index]};
}

bool EntityPool::destroy(Entity entity) {
    if (!alive(entity)) {
        return false;
    }
    const std::uint32_t index = entity.index();
    _alive[index] = false;
    ++_generations[index];
    _free_indices.push_back(index);
    return true;
}

bool EntityPool::alive(Entity entity) const noexcept {
    const std::uint32_t index = entity.index();
    return index < _generations.size() && _alive[index] &&
           _generations[index] == entity.generation();
}

std::size_t EntityPool::size() const noexcept { return _generations.size() - _free_indices.size(); }

} // namespace engine::core
