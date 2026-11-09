#pragma once

#include <string_view>

#include "engine/core/console/CVars.hpp"
#include "engine/core/console/Console.hpp"
#include "lua/Sol.hpp"

namespace engine::script {

void bind_cvars(sol::state& lua, core::console::CVars& cvars);

core::console::ExecutionResult evaluate(sol::state& lua, const sol::table& scope,
                                        std::string_view code);

} // namespace engine::script
