#include "NameList.hpp"

#include <cstddef>
#include <format>
#include <stdexcept>
#include <utility>

#include "lua/LuaErrors.hpp"

namespace engine::script {

std::vector<std::string> checked_names(sol::this_state state, std::string_view label,
                                       const NameRule& rule, const sol::object& names) {
    const std::string where = caller_location(state);
    if (names.get_type() != sol::type::table) {
        throw std::runtime_error(std::format("{}{} expects a list of file names", where, label));
    }
    const auto list = names.as<sol::table>();
    std::vector<std::string> checked;
    for (std::size_t index = 1; index <= list.size(); ++index) {
        const sol::object entry = list[index];
        if (entry.get_type() != sol::type::string) {
            throw std::runtime_error(
                std::format("{}{}: entry {} must be a string", where, label, index));
        }
        auto name = entry.as<std::string>();
        if (!rule.is_valid(name)) {
            throw std::runtime_error(std::format("{}{}: '{}' is not a valid file name: {}", where,
                                                 label, name, rule.description));
        }
        checked.push_back(std::move(name));
    }
    return checked;
}

} // namespace engine::script
