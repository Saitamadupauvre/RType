#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include "engine/core/Entity.hpp"

namespace engine::core {

/**
 * @brief Storage of one component type, laid out as a sparse set.
 *
 * A sparse array maps an entity index to a position in two packed dense arrays
 * holding the entities and their components. Insertion, removal and lookup are
 * O(1), and iteration walks contiguous memory without holes.
 *
 * A pool holds at most one component per entity index. A component stored for
 * a destroyed entity is never returned for a newer entity reusing its index.
 *
 * @tparam T Component type, movable.
 * @note Not thread safe. Iterators and references are invalidated by insert,
 * emplace, erase and clear.
 */
template <typename T> class ComponentPool {
public:
    using iterator = typename std::vector<T>::iterator;
    using const_iterator = typename std::vector<T>::const_iterator;

    /**
     * @brief Stores a component for an entity, replacing any previous one.
     * @param entity Owner of the component.
     * @param component Value to store.
     * @return Reference to the stored component.
     */
    T& insert(Entity entity, T component) { return emplace(entity, std::move(component)); }

    /**
     * @brief Builds a component in place for an entity, replacing any previous one.
     * @param entity Owner of the component.
     * @param args Arguments forwarded to the constructor of T.
     * @return Reference to the stored component.
     */
    template <typename... Args> T& emplace(Entity entity, Args&&... args) {
        T value(std::forward<Args>(args)...);
        const std::uint32_t slot = slot_of_index(entity.index());

        if (slot != no_slot) {
            _entities[slot] = entity;
            _values[slot] = std::move(value);
            return _values[slot];
        }
        if (_sparse.size() <= entity.index()) {
            _sparse.resize(static_cast<std::size_t>(entity.index()) + 1, no_slot);
        }

        _entities.reserve(_entities.size() + 1);
        _values.reserve(_values.size() + 1);
        _sparse[entity.index()] = static_cast<std::uint32_t>(_values.size());
        _entities.push_back(entity);
        _values.push_back(std::move(value));
        return _values.back();
    }

    /**
     * @brief Removes the component of an entity.
     * @param entity Owner of the component.
     * @return True if a component was removed, false if the entity had none.
     * @note The last component takes the place of the removed one.
     */
    bool erase(Entity entity) {
        const std::uint32_t slot = slot_of(entity);
        if (slot == no_slot) {
            return false;
        }
        const std::size_t last = _values.size() - 1;
        if (slot != last) {
            _entities[slot] = _entities[last];
            _values[slot] = std::move(_values[last]);
            _sparse[_entities[slot].index()] = slot;
        }
        _entities.pop_back();
        _values.pop_back();
        _sparse[entity.index()] = no_slot;
        return true;
    }

    /**
     * @param entity Entity to look up.
     * @return True if the pool holds a component for exactly this handle.
     */
    [[nodiscard]] bool contains(Entity entity) const noexcept { return slot_of(entity) != no_slot; }

    /**
     * @param entity Owner of the component.
     * @return Reference to the component.
     * @throws std::out_of_range If the entity has no component in this pool.
     */
    [[nodiscard]] T& get(Entity entity) { return _values[checked_slot(entity)]; }

    /** @copydoc get(Entity) */
    [[nodiscard]] const T& get(Entity entity) const { return _values[checked_slot(entity)]; }

    /**
     * @param entity Owner of the component.
     * @return Pointer to the component, or nullptr if the entity has none.
     */
    [[nodiscard]] T* find(Entity entity) noexcept {
        const std::uint32_t slot = slot_of(entity);
        return slot == no_slot ? nullptr : &_values[slot];
    }

    /** @copydoc find(Entity) */
    [[nodiscard]] const T* find(Entity entity) const noexcept {
        const std::uint32_t slot = slot_of(entity);
        return slot == no_slot ? nullptr : &_values[slot];
    }

    /** @return Number of components stored. */
    [[nodiscard]] std::size_t size() const noexcept { return _values.size(); }

    /** @return True if no component is stored. */
    [[nodiscard]] bool empty() const noexcept { return _values.empty(); }

    /** @brief Removes every component. */
    void clear() noexcept {
        _sparse.clear();
        _entities.clear();
        _values.clear();
    }

    /**
     * @return Owners of the components, in the same order as iteration.
     */
    [[nodiscard]] std::span<const Entity> entities() const noexcept { return _entities; }

    [[nodiscard]] iterator begin() noexcept { return _values.begin(); }
    [[nodiscard]] iterator end() noexcept { return _values.end(); }
    [[nodiscard]] const_iterator begin() const noexcept { return _values.begin(); }
    [[nodiscard]] const_iterator end() const noexcept { return _values.end(); }

private:
    static constexpr std::uint32_t no_slot = std::numeric_limits<std::uint32_t>::max();

    [[nodiscard]] std::uint32_t slot_of_index(std::uint32_t index) const noexcept {
        return index < _sparse.size() ? _sparse[index] : no_slot;
    }

    [[nodiscard]] std::uint32_t slot_of(Entity entity) const noexcept {
        const std::uint32_t slot = slot_of_index(entity.index());
        return slot != no_slot && _entities[slot] == entity ? slot : no_slot;
    }

    [[nodiscard]] std::uint32_t checked_slot(Entity entity) const {
        const std::uint32_t slot = slot_of(entity);
        if (slot == no_slot) {
            throw std::out_of_range("entity has no component in this pool");
        }
        return slot;
    }

    std::vector<std::uint32_t> _sparse;
    std::vector<Entity> _entities;
    std::vector<T> _values;
};

} // namespace engine::core
