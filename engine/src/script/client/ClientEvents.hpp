#pragma once

#include <map>
#include <string>
#include <vector>

#include "engine/script/EventCommand.hpp"
#include "lua/Sol.hpp"

namespace engine::script {

class RuntimeContext;

class ClientEvents {
public:
    explicit ClientEvents(RuntimeContext& context);

    void bind();
    bool dispatch(const EventCommand& event);

private:
    void on(sol::this_state state, const sol::object& name, const sol::object& handler);
    [[nodiscard]] bool notify_scene_loaded(const EventCommand& event) const;

    RuntimeContext& _context;
    std::map<std::string, std::vector<sol::protected_function>, std::less<>> _handlers;
};

} // namespace engine::script
