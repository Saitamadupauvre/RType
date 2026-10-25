#include "engine/core/SystemScheduler.hpp"

#include <algorithm>
#include <exception>
#include <utility>

#include "engine/core/log/Log.hpp"

namespace engine::core {

SystemAlreadyRegistered::SystemAlreadyRegistered(std::string_view name)
    : std::logic_error("system already registered: " + std::string(name)) {}

SystemHandle SystemScheduler::add(std::string name, int order, SystemFunction function) {
    if (_running) {
        throw std::logic_error("cannot add a system while the scheduler is running");
    }
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

bool SystemScheduler::remove(SystemHandle handle) {
    const auto found = find(handle);
    if (found == _systems.end()) {
        return false;
    }
    if (_running) {
        found->removed = true;
    } else {
        _systems.erase(found);
    }
    return true;
}

void SystemScheduler::run(EntityManager& manager, float dt) {
    _running = true;
    for (System& system : _systems) {
        if (system.enabled && !system.removed) {
            run_system(system, manager, dt);
        }
    }
    _running = false;
    std::erase_if(_systems, [](const System& system) { return system.removed; });
}

std::vector<SystemScheduler::System>::iterator SystemScheduler::find(SystemHandle handle) {
    return std::ranges::find_if(_systems, [&](const System& system) {
        return system.handle.id == handle.id && !system.removed;
    });
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
