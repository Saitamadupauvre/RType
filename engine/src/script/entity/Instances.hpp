#pragma once

#include <span>
#include <string_view>
#include <vector>

#include "RuntimeContext.hpp"
#include "engine/core/FieldRegistry.hpp"
#include "engine/script/Instance.hpp"

namespace engine::script {

[[nodiscard]] core::FieldRef find_field_key(const core::FieldRegistry& fields,
                                            std::string_view key);

[[nodiscard]] std::vector<Instance> capture_instances(RuntimeContext& context);
[[nodiscard]] std::vector<InstanceIssue> spawn_instances(RuntimeContext& context,
                                                         std::span<const Instance> instances);

} // namespace engine::script
