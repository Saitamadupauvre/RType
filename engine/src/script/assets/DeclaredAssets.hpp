#pragma once

#include <cstddef>
#include <functional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "lua/Sol.hpp"

namespace engine::script {

inline constexpr std::size_t max_asset_declaration_length = 63;

[[nodiscard]] bool is_valid_declared_asset_name(std::string_view name) noexcept;

class DeclaredAssets {
public:
    DeclaredAssets(std::string_view function, std::string_view kind) noexcept
        : _function(function), _kind(kind) {}

    void allow_declarations(bool allowed) noexcept { _declarations_allowed = allowed; }

    void declare(sol::this_state state, const sol::object& names);
    bool add(std::string_view name);

    [[nodiscard]] bool contains(std::string_view name) const;
    [[nodiscard]] std::vector<std::string> names() const;
    [[nodiscard]] std::string_view function() const noexcept { return _function; }
    [[nodiscard]] std::string_view kind() const noexcept { return _kind; }

private:
    std::string_view _function;
    std::string_view _kind;
    std::set<std::string, std::less<>> _names;
    bool _declarations_allowed{false};
};

} // namespace engine::script
