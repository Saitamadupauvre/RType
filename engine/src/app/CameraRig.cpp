#include "CameraRig.hpp"

#include <cmath>
#include <cstdint>
#include <optional>
#include <string_view>
#include <variant>

#include "engine/core/Entity.hpp"

namespace engine::app {

namespace {

struct SceneCamera {
    core::Vec2 center;
    core::Camera camera;
};

template <typename T>
std::optional<T> field_of(const network::EntitySnapshot& snapshot, std::string_view component,
                          std::string_view field) {
    const core::FieldValue* value = snapshot.find(component, field);
    if (value == nullptr) {
        return std::nullopt;
    }
    const T* typed = std::get_if<T>(value);
    if (typed == nullptr) {
        return std::nullopt;
    }
    return *typed;
}

float valid_zoom(float zoom) { return std::isfinite(zoom) && zoom > 0.F ? zoom : 1.F; }

SceneCamera scene_camera_of(const network::EntitySnapshot& snapshot, core::Vec2 viewport) {
    const auto position = field_of<core::Vec2>(snapshot, "Position", "pos");
    return SceneCamera{
        .center = position.value_or(viewport / 2.F),
        .camera = core::Camera{
            .zoom = valid_zoom(field_of<float>(snapshot, "Camera", "zoom").value_or(1.F)),
            .follow = {},
            .bounds_origin =
                field_of<core::Vec2>(snapshot, "Camera", "bounds_origin").value_or(core::Vec2{}),
            .bounds_size =
                field_of<core::Vec2>(snapshot, "Camera", "bounds_size").value_or(core::Vec2{})}};
}

std::optional<SceneCamera> find_scene_camera(const std::vector<network::EntitySnapshot>& entities,
                                             core::Vec2 viewport) {
    const network::EntitySnapshot* first = nullptr;
    for (const network::EntitySnapshot& snapshot : entities) {
        if (is_camera(snapshot) && (first == nullptr || snapshot.entity_id < first->entity_id)) {
            first = &snapshot;
        }
    }
    if (first == nullptr) {
        return std::nullopt;
    }
    return scene_camera_of(*first, viewport);
}

std::optional<core::Vec2> position_of(const std::vector<network::EntitySnapshot>& entities,
                                      core::Entity::Id id) {
    const std::uint32_t index = core::Entity{id}.index();
    for (const network::EntitySnapshot& snapshot : entities) {
        if (snapshot.entity_id == index) {
            return field_of<core::Vec2>(snapshot, "Position", "pos");
        }
    }
    return std::nullopt;
}

core::CameraView follow_view(core::Vec2 target, const script::CameraOverride& local,
                             const std::optional<SceneCamera>& scene, core::Vec2 viewport) {
    const core::CameraView view{.center = target, .zoom = valid_zoom(local.view.zoom)};
    if (!scene || !scene->camera.bounded()) {
        return view;
    }
    return core::clamp_to_bounds(view, viewport, scene->camera.bounds_origin,
                                 scene->camera.bounds_size);
}

} // namespace

bool is_camera(const network::EntitySnapshot& snapshot) {
    return snapshot.find("Camera", "zoom") != nullptr;
}

core::CameraView resolve_camera(const std::vector<network::EntitySnapshot>& entities,
                                const script::CameraOverride& local, core::Vec2 viewport) {
    if (local.kind == script::CameraOverride::Kind::Fixed) {
        return {.center = local.view.center, .zoom = valid_zoom(local.view.zoom)};
    }
    const auto scene = find_scene_camera(entities, viewport);
    if (local.kind == script::CameraOverride::Kind::Follow) {
        if (const auto target = position_of(entities, local.entity)) {
            return follow_view(*target, local, scene, viewport);
        }
    }
    if (scene) {
        return core::camera_view(scene->center, scene->camera, viewport);
    }
    return core::default_view(viewport);
}

} // namespace engine::app
