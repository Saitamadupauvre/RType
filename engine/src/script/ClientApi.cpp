#include "engine/script/ClientApi.hpp"

#include <exception>
#include <format>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "RuntimeContext.hpp"
#include "engine/core/log/Log.hpp"
#include "lua/LuaErrors.hpp"
#include "lua/LuaFieldValue.hpp"

namespace engine::script {

namespace {

namespace log = engine::core::log;

} // namespace

struct ClientApi::State {
    RuntimeContext* context = nullptr;
    std::map<std::string, std::vector<sol::protected_function>, std::less<>> handlers;

    void on(sol::this_state state, const sol::object& name, const sol::object& handler);
    bool dispatch(const EventCommand& event);
};

void ClientApi::State::on(sol::this_state state, const sol::object& name,
                          const sol::object& handler) {
    if (name.get_type() != sol::type::string) {
        throw std::runtime_error(
            std::format("{}on: event name must be a string", caller_location(state)));
    }
    auto event = name.as<std::string>();
    if (!context->events.contains(event)) {
        throw std::runtime_error(std::format("{}on: unknown event '{}', declare it with event",
                                             caller_location(state), event));
    }
    if (handler.get_type() != sol::type::function) {
        throw std::runtime_error(std::format("{}on: handler of event '{}' must be a function",
                                             caller_location(state), event));
    }
    handlers[std::move(event)].push_back(handler.as<sol::protected_function>());
}

bool ClientApi::State::dispatch(const EventCommand& event) {
    const auto found = handlers.find(event.name);
    if (found == handlers.end()) {
        return true;
    }
    const sol::this_state state{context->lua.lua_state()};
    bool success = true;
    for (const sol::protected_function& handler : found->second) {
        sol::table arguments = context->lua.create_table();
        for (const EventArgument& argument : event.arguments) {
            arguments[argument.key] = to_lua(state, argument.value);
        }
        success = report(handler(arguments)) && success;
    }
    return success;
}

ClientApi::ClientApi() : _state(std::make_unique<State>()) {}

ClientApi::~ClientApi() = default;

std::string_view ClientApi::section() const noexcept { return "client"; }

void ClientApi::install(RuntimeContext& context) {
    _state->context = &context;
    context.lua.set_function(
        "on", [this](sol::this_state state, const sol::object& name, const sol::object& handler) {
            _state->on(state, name, handler);
        });
}

bool ClientApi::dispatch(const EventCommand& event) noexcept {
    try {
        return _state->dispatch(event);
    } catch (const std::exception& exception) {
        log::error("lua: event '{}': {}", event.name, exception.what());
    } catch (...) {
        log::error("lua: event '{}': unknown error", event.name);
    }
    return false;
}

} // namespace engine::script
