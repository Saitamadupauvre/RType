#pragma once

#include <filesystem>

#include "RuntimeContext.hpp"
#include "engine/script/Scene.hpp"

namespace engine::script {

[[nodiscard]] SceneLoad read_scene(const RuntimeContext& context,
                                   const std::filesystem::path& file);

} // namespace engine::script
