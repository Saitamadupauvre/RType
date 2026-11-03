#include "SceneSwitch.hpp"

#include <format>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <utility>

#include "GameFolder.hpp"
#include "SceneFile.hpp"
#include "entity/Instances.hpp"
#include "lua/LuaErrors.hpp"

namespace engine::script {

namespace {

[[noreturn]] void fail(sol::this_state state, std::string_view message) {
    throw std::runtime_error(std::format("{}load_scene: {}", caller_location(state), message));
}

std::string checked_scene_name(const RuntimeContext& context, sol::this_state state,
                               const sol::object& name) {
    if (name.get_type() != sol::type::string) {
        fail(state, "scene name must be a string");
    }
    auto scene = name.as<std::string>();
    if (!is_scene_name(scene)) {
        fail(state, std::format("'{}' is not a valid scene name: 1 to 63 letters, digits, '_' "
                                "or '-'",
                                scene));
    }
    if (context.pending_scene) {
        fail(state, std::format("cannot load '{}', scene '{}' is already pending", scene,
                                context.pending_scene->name));
    }
    return scene;
}

bool is_persistent(const RuntimeContext& context, core::Entity entity) {
    const auto type = context.entities.type_of(entity);
    const Declaration* declaration = type ? context.find_declaration(*type) : nullptr;
    return declaration != nullptr && declaration->persistent;
}

void destroy_scene_entities(RuntimeContext& context) {
    for (const core::Entity entity : context.entities.live_entities()) {
        if (!is_persistent(context, entity)) {
            context.entities.doom(entity);
        }
    }
    context.entities.flush_destroyed();
}

} // namespace

void request_scene(RuntimeContext& context, sol::this_state state, const sol::object& name) {
    std::string scene = checked_scene_name(context, state, name);
    const std::optional<std::filesystem::path>& game = context.game;
    if (!game) {
        fail(state, "no game folder is loaded");
    }
    SceneLoad load = read_scene(context, scene_file(*game, scene));
    if (!load.ok()) {
        fail(state, std::format("cannot load scene '{}': {}", scene, load.error));
    }
    context.pending_scene = PendingScene{.name = std::move(scene), .scene = std::move(load.scene)};
}

std::optional<std::string> switch_to_pending_scene(RuntimeContext& context) {
    std::optional<PendingScene> pending = std::exchange(context.pending_scene, std::nullopt);
    if (!pending) {
        return std::nullopt;
    }
    destroy_scene_entities(context);
    std::ignore = spawn_instances(context, pending->scene.instances);
    context.events.emit_scene_loaded(pending->name);
    const sol::protected_function callback = context.main_callback("on_scene_loaded");
    if (callback.valid()) {
        report(callback(pending->name));
    }
    return std::move(pending->name);
}

} // namespace engine::script
