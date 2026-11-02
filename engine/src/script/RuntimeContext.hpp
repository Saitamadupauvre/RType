#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>

#include "audio/Audio.hpp"
#include "engine/core/FieldRegistry.hpp"
#include "entity/Entities.hpp"
#include "event/Events.hpp"
#include "loading/Declarations.hpp"
#include "lua/Sol.hpp"

namespace engine::script {

class RuntimeContext {
public:
    RuntimeContext();

    [[nodiscard]] const Declaration* find_declaration(std::string_view name) const;
    [[nodiscard]] sol::protected_function main_callback(std::string_view name) const;

    core::FieldRegistry fields;
    sol::state lua;
    sol::table types;
    Entities entities;
    Audio audio;
    Events events;
    std::map<std::string, Declaration, std::less<>> declarations;
    std::optional<sol::environment> main;
};

} // namespace engine::script
