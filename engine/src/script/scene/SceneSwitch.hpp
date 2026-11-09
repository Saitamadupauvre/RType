#pragma once

#include <optional>
#include <string>

#include "RuntimeContext.hpp"
#include "lua/Sol.hpp"

namespace engine::script {

void request_scene(RuntimeContext& context, sol::this_state state, const sol::object& name);

std::optional<std::string> switch_to_pending_scene(RuntimeContext& context);

} // namespace engine::script
