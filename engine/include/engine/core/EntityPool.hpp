#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "engine/core/Entity.hpp"
#include "engine/core/Export.hpp"

namespace engine::core {

/**
 * @brief Creates and destroys entities, recycling the slots of destroyed ones.
 *
 * A destroyed slot is reused by a later create() with its generation bumped,
 * so handles to the destroyed entity are reported as not alive.
 *
 * @note Not thread safe: use one pool per thread or synchronize externally.
 */
class EntityPool {
public:
    /**
     * @brief Creates a new entity, reusing a destroyed slot when one is free.
     * @return A handle that stays alive until destroy() is called with it.
     */
    ENGINE_CORE_EXPORT Entity create();

    /**
     * @brief Destroys an entity and frees its slot for reuse.
     * @param entity Handle to destroy. A stale or unknown handle is ignored.
     * @return true if the entity was alive and is now destroyed, false otherwise.
     */
    ENGINE_CORE_EXPORT bool destroy(Entity entity);

    /**
     * @brief Tells whether a handle refers to a living entity of this pool.
     * @param entity Handle to check.
     * @return false for destroyed, stale or unknown handles.
     */
    [[nodiscard]] ENGINE_CORE_EXPORT bool alive(Entity entity) const noexcept;

    /** @return The number of living entities. */
    [[nodiscard]] ENGINE_CORE_EXPORT std::size_t size() const noexcept;

private:
    std::vector<std::uint32_t> _generations;
    std::vector<bool> _alive;
    std::vector<std::uint32_t> _free_indices;
};

} // namespace engine::core
