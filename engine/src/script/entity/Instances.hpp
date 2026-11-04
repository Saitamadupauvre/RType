#pragma once

#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "RuntimeContext.hpp"
#include "engine/core/ecs/FieldRegistry.hpp"
#include "engine/script/Instance.hpp"

namespace engine::script {

[[nodiscard]] core::FieldRef find_field_key(const core::FieldRegistry& fields,
                                            std::string_view key);

[[nodiscard]] std::vector<Instance> capture_instances(RuntimeContext& context);
[[nodiscard]] std::vector<InstanceIssue> spawn_instances(RuntimeContext& context,
                                                         std::span<const Instance> instances);
[[nodiscard]] Preview preview_instances(RuntimeContext& context, core::EntityManager& world,
                                        std::span<const Instance> instances);
[[nodiscard]] std::optional<core::FieldValue>
default_value(RuntimeContext& context, std::string_view type, std::string_view key);

} // namespace engine::script
