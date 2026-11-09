#pragma once

#include <optional>

#include <nlohmann/json.hpp>

#include "engine/core/ecs/FieldRegistry.hpp"
#include "engine/core/math/Vec2.hpp"

namespace engine::script {

using Json = nlohmann::ordered_json;

core::FieldValue field_from_json(const Json& value, core::FieldKind kind,
                                 std::optional<core::FieldKind> element);

core::Vec2 vec2_from_json(const Json& value);

Json field_to_json(const core::FieldValue& value);

Json vec2_to_json(core::Vec2 value);

} // namespace engine::script
