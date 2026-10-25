#pragma once

#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "engine/core/EntityManager.hpp"
#include "engine/core/Export.hpp"

namespace engine::core {

/** @brief Thrown when a system is added with a name that is already in use. */
class ENGINE_CORE_EXPORT SystemAlreadyRegistered : public std::logic_error {
public:
    /** @param name The duplicated system name. */
    explicit SystemAlreadyRegistered(std::string_view name);
};

/** @brief Identifies one system added to a SystemScheduler. */
struct SystemHandle {
    std::uint64_t id{0};
};

/** @brief Function called once per tick with the entity manager and the fixed time step. */
using SystemFunction = std::function<void(EntityManager&, float)>;

/**
 * @brief Runs systems over an entity manager, always in the same order.
 *
 * Systems run by ascending order value. Systems with the same order value run in the order they
 * were added.
 *
 * @note Not thread safe.
 */
class ENGINE_CORE_EXPORT SystemScheduler {
public:
    /**
     * @brief Adds a system.
     *
     * @param name Unique name of the system.
     * @param order Position in the tick: lower values run first.
     * @param function Called on every run.
     * @return A handle identifying the system.
     * @throws SystemAlreadyRegistered If a system with this name already exists.
     * @throws std::logic_error If called while run is executing.
     */
    SystemHandle add(std::string name, int order, SystemFunction function);

    /**
     * @brief Removes a system and frees its name.
     *
     * Safe to call from inside a running system, including on itself: the removed system is not
     * called again, and if it had not run yet in the current tick it is skipped.
     *
     * @param handle Handle returned by add.
     * @return true if the system existed and is now removed, false for an unknown or already
     * removed handle.
     */
    bool remove(SystemHandle handle);

    /**
     * @brief Calls every enabled system once, in order.
     *
     * A system that throws is logged and disabled: it is never called again, and the remaining
     * systems still run. A disabled system keeps its name reserved.
     *
     * @param manager Passed to each system.
     * @param dt Fixed time step in seconds, passed to each system.
     */
    void run(EntityManager& manager, float dt);

private:
    struct System {
        SystemHandle handle;
        std::string name;
        int order;
        SystemFunction function;
        bool enabled{true};
        bool removed{false};
    };

    static void run_system(System& system, EntityManager& manager, float dt);
    static void disable(System& system, std::string_view reason) noexcept;
    std::vector<System>::iterator find(SystemHandle handle);

    std::vector<System> _systems;
    std::uint64_t _next_id{1};
    bool _running{false};
};

} // namespace engine::core
