#include "client/ClientEvents.hpp"

#include <format>
#include <stdexcept>
#include <utility>
#include <variant>

#include "RuntimeContext.hpp"
#include "event/Events.hpp"
#include "lua/LuaErrors.hpp"
#include "lua/LuaFieldValue.hpp"

namespace engine::script {

ClientEvents::ClientEvents(RuntimeContext& context) : _context(context) {}

void ClientEvents::bind() {
    _context.lua.set_function("on",
                              [this](sol::this_state state, const sol::object& name,
                                     const sol::object& handler) { on(state, name, handler); });
}

void ClientEvents::on(sol::this_state state, const sol::object& name, const sol::object& handler) {
    if (name.get_type() != sol::type::string) {
        throw std::runtime_error(
            std::format("{}on: event name must be a string", caller_location(state)));
    }
    auto event = name.as<std::string>();
    if (!_context.events.contains(event)) {
        throw std::runtime_error(std::format("{}on: unknown event '{}', declare it with event",
                                             caller_location(state), event));
    }
    if (handler.get_type() != sol::type::function) {
        throw std::runtime_error(std::format("{}on: handler of event '{}' must be a function",
                                             caller_location(state), event));
    }
    _handlers[std::move(event)].push_back(handler.as<sol::protected_function>());
}

bool ClientEvents::notify_scene_loaded(const EventCommand& event) const {
    if (event.name != scene_loaded_event) {
        return true;
    }
    const sol::protected_function callback = _context.main_callback("on_scene_loaded");
    if (!callback.valid()) {
        return true;
    }
    for (const EventArgument& argument : event.arguments) {
        const auto* scene = std::get_if<std::string>(&argument.value);
        if (argument.key == scene_loaded_key && scene != nullptr) {
            return report(callback(*scene));
        }
    }
    return true;
}

bool ClientEvents::dispatch(const EventCommand& event) {
    bool success = notify_scene_loaded(event);
    const auto found = _handlers.find(event.name);
    if (found == _handlers.end()) {
        return success;
    }
    const sol::this_state state{_context.lua.lua_state()};
    for (const sol::protected_function& handler : found->second) {
        sol::table arguments = _context.lua.create_table();
        for (const EventArgument& argument : event.arguments) {
            arguments[argument.key] = to_lua(state, argument.value);
        }
        success = report(handler(arguments)) && success;
    }
    return success;
}

} // namespace engine::script
