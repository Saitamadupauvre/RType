#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace engine::script {

struct SceneAssets {
    std::vector<std::string> textures;
    std::vector<std::string> shaders;
};

SceneAssets scene_assets(const std::filesystem::path& file);

} // namespace engine::script
