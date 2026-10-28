#include "engine/script/ScriptRuntimeBuilder.hpp"

#include <format>
#include <stdexcept>
#include <typeindex>
#include <typeinfo>
#include <unordered_set>
#include <utility>

#include "engine/script/ScriptRuntime.hpp"

namespace engine::script {

namespace {

void check_sets(const std::vector<std::unique_ptr<ApiSet>>& sets) {
    std::unordered_set<std::type_index> added;
    std::string_view section;
    for (const auto& set : sets) {
        const ApiSet& api = *set;
        if (!added.insert(typeid(api)).second) {
            throw std::invalid_argument(
                std::format("script API set {} is added twice", typeid(api).name()));
        }
        if (api.section().empty()) {
            continue;
        }
        if (!section.empty()) {
            throw std::invalid_argument(
                std::format("script API sets reading the '{}' and '{}' sections cannot be combined",
                            section, api.section()));
        }
        section = api.section();
    }
}

} // namespace

ScriptRuntimeBuilder::ScriptRuntimeBuilder() = default;

ScriptRuntimeBuilder::~ScriptRuntimeBuilder() = default;

ScriptRuntimeBuilder::ScriptRuntimeBuilder(ScriptRuntimeBuilder&&) noexcept = default;

ScriptRuntimeBuilder& ScriptRuntimeBuilder::operator=(ScriptRuntimeBuilder&&) noexcept = default;

ScriptRuntime ScriptRuntimeBuilder::build() {
    auto sets = std::exchange(_sets, {});
    check_sets(sets);
    return ScriptRuntime(std::move(sets));
}

} // namespace engine::script
