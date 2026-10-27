#pragma once

#include <span>
#include <vector>

#include "RuntimeContext.hpp"
#include "engine/script/Instance.hpp"

namespace engine::script {

[[nodiscard]] std::vector<Instance> capture_instances(RuntimeContext& context);
[[nodiscard]] std::vector<InstanceIssue> spawn_instances(RuntimeContext& context,
                                                         std::span<const Instance> instances);

} // namespace engine::script
