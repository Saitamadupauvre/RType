#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Sol.hpp"

namespace engine::script {

class ModuleLoader {
public:
    explicit ModuleLoader(std::filesystem::path root);

    sol::object require(sol::this_state state, std::string_view name);

private:
    sol::object load(sol::this_state state, const std::string& name);

    std::filesystem::path _root;
    std::unordered_map<std::string, sol::object> _cache;
    std::vector<std::string> _loading;
};

} // namespace engine::script
