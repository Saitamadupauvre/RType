#pragma once

#include <optional>

#include "lua/Sol.hpp"

namespace engine::script {

[[nodiscard]] bool is_nil(const sol::object& value);

[[nodiscard]] std::optional<float> finite_number(const sol::object& value);

[[nodiscard]] std::optional<float> positive_number(const sol::object& value);

[[nodiscard]] std::optional<lua_Integer> non_negative_integer(sol::this_state state,
                                                              const sol::object& value);

} // namespace engine::script
