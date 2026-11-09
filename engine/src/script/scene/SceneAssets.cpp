#include "SceneAssets.hpp"

#include <exception>
#include <string_view>

#include "JsonFile.hpp"

namespace engine::script {

namespace {

constexpr std::string_view texture_key = "Sprite.path";
constexpr std::string_view shader_key = "Shader.path";

void collect(const Json& instance, SceneAssets& assets) {
    if (!instance.is_object() || !instance.contains("overrides") ||
        !instance["overrides"].is_object()) {
        return;
    }
    for (const auto& [key, value] : instance["overrides"].items()) {
        if (!value.is_string() || value.get_ref<const std::string&>().empty()) {
            continue;
        }
        if (key == texture_key) {
            assets.textures.push_back(value.get<std::string>());
        } else if (key == shader_key) {
            assets.shaders.push_back(value.get<std::string>());
        }
    }
}

} // namespace

SceneAssets scene_assets(const std::filesystem::path& file) {
    SceneAssets assets;
    try {
        const Json scene = read_json_file(file);
        if (!scene.is_object() || !scene.contains("instances") || !scene["instances"].is_array()) {
            return assets;
        }
        for (const Json& instance : scene["instances"]) {
            collect(instance, assets);
        }
    } catch (const std::exception&) {
        return {};
    }
    return assets;
}

} // namespace engine::script
