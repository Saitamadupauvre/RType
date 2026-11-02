#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "lua/Sol.hpp"

namespace engine::script {

struct NameRule {
    bool (*is_valid)(std::string_view name) noexcept;
    std::string_view description;
};

[[nodiscard]] std::vector<std::string> checked_names(sol::this_state state, std::string_view label,
                                                     const NameRule& rule,
                                                     const sol::object& names);

} // namespace engine::script
