#include "engine/core/SystemScheduler.hpp"

#include <algorithm>
#include <exception>
#include <utility>

#include "engine/core/log/Log.hpp"

namespace engine::core {

SystemAlreadyRegistered::SystemAlreadyRegistered(std::string_view name)
    : std::logic_error("system already registered: " + std::string(name)) {}

SystemHandle SystemScheduler::add(std::string name, int order, SystemFunction function) {
    const auto same_name = [&](const System& system) { return system.name == name; };
    if (std::ranges::any_of(_systems, same_name)) {
        throw SystemAlreadyRegistered(name);
    }

    const SystemHandle handle{_next_id++};
    const auto position = std::ranges::upper_bound(_systems, order, {}, &System::order);
    _systems.insert(position, System{.handle = handle,
                                     .name = std::move(name),
                                     .order = order,
                                     .function = std::move(function)});
    return handle;
}

void SystemScheduler::run(EntityManager& manager, float dt) {
    for (System& system : _systems) {
        if (system.enabled) {
            run_system(system, manager, dt);
        }
    }
}

void SystemScheduler::run_system(System& system, EntityManager& manager, float dt) {
    try {
        system.function(manager, dt);
    } catch (const std::exception& error) {
        disable(system, error.what());
    } catch (...) {
        disable(system, "unknown exception");
    }
}

void SystemScheduler::disable(System& system, std::string_view reason) noexcept {
    system.enabled = false;
    log::error("system '{}' threw and is now disabled: {}", system.name, reason);
}

} // namespace engine::core
