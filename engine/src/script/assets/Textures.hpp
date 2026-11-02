#pragma once

#include <cstddef>
#include <functional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "lua/Sol.hpp"

namespace engine::script {

inline constexpr std::size_t max_texture_name_length = 63;

[[nodiscard]] bool is_valid_texture_name(std::string_view name) noexcept;

class Textures {
public:
    void allow_declarations(bool allowed) noexcept { _declarations_allowed = allowed; }

    void declare(sol::this_state state, const sol::object& names);
    bool add(std::string_view name);

    [[nodiscard]] bool contains(std::string_view name) const;
    [[nodiscard]] std::vector<std::string> names() const;

private:
    std::set<std::string, std::less<>> _names;
    bool _declarations_allowed{false};
};

} // namespace engine::script
