#include "client/ClientEvents.hpp"

#include <format>
#include <stdexcept>
#include <utility>

#include "RuntimeContext.hpp"
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

bool ClientEvents::dispatch(const EventCommand& event) {
    const auto found = _handlers.find(event.name);
    if (found == _handlers.end()) {
        return true;
    }
    const sol::this_state state{_context.lua.lua_state()};
    bool success = true;
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
