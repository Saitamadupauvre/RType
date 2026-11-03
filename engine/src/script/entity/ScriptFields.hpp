#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "engine/core/FieldRegistry.hpp"
#include "engine/script/ScriptField.hpp"
#include "entity/Entities.hpp"
#include "loading/Declarations.hpp"
#include "lua/Sol.hpp"

namespace engine::script {

[[nodiscard]] std::vector<ScriptField>
infer_script_fields(sol::this_state state, const sol::table& prefab, const Entities& entities);

[[nodiscard]] std::optional<std::string_view> script_field_name(std::string_view key) noexcept;

[[nodiscard]] std::string script_field_key(std::string_view name);

[[nodiscard]] const ScriptField* find_script_field(const Declaration& declaration,
                                                   std::string_view name) noexcept;

[[nodiscard]] core::FieldValue
script_value_from_lua(sol::this_state state, const sol::object& value, const ScriptField& field);

[[nodiscard]] bool has_shape_of(const core::FieldValue& value, const ScriptField& field) noexcept;

} // namespace engine::script
