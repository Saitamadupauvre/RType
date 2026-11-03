#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>

#include "assets/DeclaredAssets.hpp"
#include "audio/Audio.hpp"
#include "engine/core/FieldRegistry.hpp"
#include "engine/script/Scene.hpp"
#include "entity/Entities.hpp"
#include "event/Events.hpp"
#include "loading/Declarations.hpp"
#include "lua/Sol.hpp"

namespace engine::script {

struct PendingScene {
    std::string name;
    Scene scene;
};

class RuntimeContext {
public:
    RuntimeContext();

    [[nodiscard]] const Declaration* find_declaration(std::string_view name) const;
    [[nodiscard]] sol::protected_function main_callback(std::string_view name) const;

    core::FieldRegistry fields;
    sol::state lua;
    sol::table types;
    DeclaredAssets textures{"textures", "texture"};
    DeclaredAssets shaders{"shaders", "shader"};
    Entities entities;
    Audio audio;
    Events events;
    std::map<std::string, Declaration, std::less<>> declarations;
    std::optional<sol::environment> main;
    std::optional<std::filesystem::path> game;
    std::optional<PendingScene> pending_scene;
};

} // namespace engine::script
