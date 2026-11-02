#pragma once

#include <string_view>

#include "engine/core/FieldRegistry.hpp"
#include "engine/core/Vec2.hpp"
#include "lua/Sol.hpp"

namespace engine::script {

sol::object to_lua(sol::this_state state, const core::FieldValue& value);

core::FieldValue from_lua(sol::this_state state, const sol::object& value,
                          const core::FieldDescriptor& field, std::string_view name);

core::FieldValue from_lua(sol::this_state state, const sol::object& value, core::FieldKind kind,
                          std::string_view label, std::string_view name);

float number_from_lua(sol::this_state state, const sol::object& value, std::string_view name);

sol::object size_to_lua(sol::this_state state, core::Vec2 size);

core::Vec2 size_from_lua(sol::this_state state, const sol::object& value, std::string_view name);

} // namespace engine::script
