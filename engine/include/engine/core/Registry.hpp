#pragma once

#include <memory>
#include <stdexcept>
#include <string_view>
#include <typeinfo>
#include <utility>
#include <vector>

#include "engine/core/ComponentPool.hpp"
#include "engine/core/ComponentTypeId.hpp"
#include "engine/core/Entity.hpp"
#include "engine/core/EntityPool.hpp"
#include "engine/core/Export.hpp"
#include "engine/core/View.hpp"

namespace engine::core {

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4275)
#endif

/** @brief Thrown when a component type is used before being registered. */
class ENGINE_CORE_EXPORT ComponentNotRegistered : public std::logic_error {
public:
    /** @param type_name Name of the offending component type. */
    explicit ComponentNotRegistered(std::string_view type_name);
};

/** @brief Thrown when a component type is registered twice in the same registry. */
class ENGINE_CORE_EXPORT ComponentAlreadyRegistered : public std::logic_error {
public:
    /** @param type_name Name of the offending component type. */
    explicit ComponentAlreadyRegistered(std::string_view type_name);
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

/**
 * @brief Owns the entities and every component pool, and queries them.
 *
 * Component types must be registered before use. Killing an entity removes all
 * its components.
 *
 * @note Not thread safe: use one registry per thread or synchronize externally.
 */
class Registry {
public:
    /**
     * @brief Creates a new entity.
     * @return A handle that stays alive until kill() is called with it.
     */
    ENGINE_CORE_EXPORT Entity spawn();

    /**
     * @brief Destroys an entity and removes all its components.
     * @param entity Handle to destroy. A stale or unknown handle is ignored.
     * @return true if the entity was alive and is now destroyed, false otherwise.
     */
    ENGINE_CORE_EXPORT bool kill(Entity entity);

    /**
     * @param entity Handle to check.
     * @return false for destroyed, stale or unknown handles.
     */
    [[nodiscard]] ENGINE_CORE_EXPORT bool alive(Entity entity) const noexcept;

    /**
     * @brief Makes a component type usable in this registry.
     * @tparam T Component type.
     * @throws ComponentAlreadyRegistered If T is already registered.
     */
    template <typename T> void register_component() {
        const ComponentTypeId id = component_type_id<T>();
        if (id >= _pools.size()) {
            _pools.resize(static_cast<std::size_t>(id) + 1);
        }
        if (_pools[id]) {
            throw ComponentAlreadyRegistered(typeid(T).name());
        }
        _pools[id] = std::make_unique<Pool<T>>();
    }

    /**
     * @brief Attaches a component to an entity, replacing any previous one.
     * @param entity Living owner of the component.
     * @param component Value to store.
     * @return Reference to the stored component.
     * @throws ComponentNotRegistered If T is not registered.
     * @throws std::invalid_argument If the entity is not alive.
     */
    template <typename T> T& add(Entity entity, T component) {
        ComponentPool<T>& pool = pool_of<T>();
        if (!alive(entity)) {
            throw std::invalid_argument("cannot add a component to a dead entity");
        }
        return pool.insert(entity, std::move(component));
    }

    /**
     * @brief Detaches a component from an entity.
     * @return true if a component was removed, false if the entity had none.
     * @throws ComponentNotRegistered If T is not registered.
     */
    template <typename T> bool remove(Entity entity) { return pool_of<T>().erase(entity); }

    /**
     * @return Reference to the component of the entity.
     * @throws ComponentNotRegistered If T is not registered.
     * @throws std::out_of_range If the entity has no such component.
     */
    template <typename T> [[nodiscard]] T& get(Entity entity) { return pool_of<T>().get(entity); }

    /**
     * @brief Builds a view over the entities owning all the listed components.
     * @tparam First First component type.
     * @tparam Rest Other component types. All types must be distinct.
     * @throws ComponentNotRegistered If one of the types is not registered.
     */
    template <typename First, typename... Rest>
        requires DistinctTypes<First, Rest...>
    [[nodiscard]] View<First, Rest...> view() {
        return View<First, Rest...>(pool_of<First>(), pool_of<Rest>()...);
    }

private:
    struct PoolBase {
        PoolBase() = default;
        PoolBase(const PoolBase&) = delete;
        PoolBase& operator=(const PoolBase&) = delete;
        virtual ~PoolBase() = default;
        virtual void erase(Entity entity) = 0;
    };

    template <typename T> struct Pool final : PoolBase {
        ComponentPool<T> pool;
        void erase(Entity entity) override { pool.erase(entity); }
    };

    template <typename T> ComponentPool<T>& pool_of() {
        const ComponentTypeId id = component_type_id<T>();
        if (id >= _pools.size() || !_pools[id]) {
            throw ComponentNotRegistered(typeid(T).name());
        }
        return static_cast<Pool<T>&>(*_pools[id]).pool;
    }

    EntityPool _entities;
    std::vector<std::unique_ptr<PoolBase>> _pools;
};

} // namespace engine::core
