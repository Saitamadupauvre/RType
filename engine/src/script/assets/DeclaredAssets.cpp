#include "DeclaredAssets.hpp"

#include <format>
#include <stdexcept>
#include <utility>

#include "engine/core/AssetCache.hpp"
#include "loading/NameList.hpp"
#include "lua/LuaErrors.hpp"

namespace engine::script {

namespace {

constexpr NameRule asset_name_rule{
    .is_valid = &is_valid_declared_asset_name,
    .description = "1 to 63 bytes of '/'-separated folders and a file, each made of letters, "
                   "digits, '_', '-' or '.' and not starting with '.'"};

} // namespace

bool is_valid_declared_asset_name(std::string_view name) noexcept {
    return name.size() <= max_asset_declaration_length && core::is_valid_asset_name(name);
}

void DeclaredAssets::declare(sol::this_state state, const sol::object& names) {
    if (!_declarations_allowed) {
        throw std::runtime_error(std::format("{}{} can only be called while the scripts load",
                                             caller_location(state), _function));
    }
    for (std::string& name : checked_names(state, _function, asset_name_rule, names)) {
        _names.insert(std::move(name));
    }
}

bool DeclaredAssets::add(std::string_view name) {
    if (!is_valid_declared_asset_name(name)) {
        return false;
    }
    _names.emplace(name);
    return true;
}

bool DeclaredAssets::contains(std::string_view name) const {
    return name.empty() || _names.contains(name);
}

std::vector<std::string> DeclaredAssets::names() const { return {_names.begin(), _names.end()}; }

} // namespace engine::script
