#pragma once

#include <functional>
#include <span>
#include <tuple>
#include <type_traits>

#include "engine/core/ComponentPool.hpp"
#include "engine/core/Entity.hpp"

namespace engine::core {

namespace detail {

template <typename... Ts> inline constexpr bool all_distinct = true;

template <typename T, typename... Ts>
inline constexpr bool all_distinct<T, Ts...> =
    (!std::is_same_v<T, Ts> && ...) && all_distinct<Ts...>;

} // namespace detail

/** @brief Satisfied when every type of the list appears only once. */
template <typename... Ts>
concept DistinctTypes = detail::all_distinct<Ts...>;

/**
 * @brief Iteration over the entities owning every listed component type.
 *
 * The query walks the smallest of the pools and skips entities missing one of
 * the other components.
 *
 * @tparam First First component type.
 * @tparam Rest Other component types. All types must be distinct.
 * @note Adding or removing components of the listed types while iterating is
 * undefined. Queries must not outlive the EntityManager that created them.
 */
template <typename First, typename... Rest>
    requires DistinctTypes<First, Rest...>
class ComponentQuery {
public:
    /**
     * @brief Builds a query over existing pools.
     * @param first Pool of the first component type.
     * @param rest Pools of the other component types.
     */
    ComponentQuery(ComponentPool<First>& first, ComponentPool<Rest>&... rest)
        : _pools(first, rest...) {}

    /**
     * @brief Calls a function for every entity owning all the component types.
     * @param fn Callable invoked as fn(Entity, First&, Rest&...).
     */
    template <typename Fn> void each(Fn&& fn) {
        for (const Entity entity : smallest_pool_entities()) {
            visit(entity, fn);
        }
    }

private:
    [[nodiscard]] std::span<const Entity> smallest_pool_entities() const noexcept {
        std::span<const Entity> smallest = std::get<0>(_pools).entities();
        std::apply(
            [&](const auto&... pool) {
                ((pool.size() < smallest.size() ? (smallest = pool.entities(), 0) : 0), ...);
            },
            _pools);
        return smallest;
    }

    template <typename Fn> void visit(Entity entity, Fn& fn) {
        std::apply(
            [&](auto&... pool) {
                const std::tuple found{pool.find(entity)...};
                const bool owns_all =
                    std::apply([](auto*... component) { return (component && ...); }, found);
                if (owns_all) {
                    std::apply([&](auto*... component) { std::invoke(fn, entity, *component...); },
                               found);
                }
            },
            _pools);
    }

    std::tuple<ComponentPool<First>&, ComponentPool<Rest>&...> _pools;
};

} // namespace engine::core
