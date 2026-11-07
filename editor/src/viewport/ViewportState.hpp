#pragma once

#include <optional>
#include <string>
#include <vector>

#include "engine/core/math/Vec2.hpp"
#include "engine/editor/SceneDocument.hpp"
#include "viewport/Handles.hpp"
#include "viewport/SceneItems.hpp"
#include "viewport/ViewCamera.hpp"

namespace editor {

/** @brief An instance being dragged: where it was grabbed, where it started and where it is shown. */
struct ViewDrag {
    engine::editor::InstanceId id;
    engine::core::Vec2 grab;
    engine::core::Vec2 start;
    engine::core::Vec2 preview;
};

/** @brief A scale or rotate handle being dragged, with the item as it was when grabbed. */
struct HandleDrag {
    Handle handle = Handle::scale;
    SceneItem item;
    engine::core::Vec2 scale{1.F, 1.F};
    float rotation = 0.F;
};

/** @brief What the viewport draws this frame, read by the renderer through Clay's custom data. */
struct ViewportFrame {
    ViewCamera camera;
    std::vector<SceneItem> items;
    GridLines grid;
    std::optional<engine::editor::InstanceId> selection;
    WorldBox game_frame;
    bool show_names = true;
    bool show_hitboxes = false;
    /** @brief Where a prefab dragged from the sidebar would be placed. */
    std::optional<WorldBox> ghost;
};

/** @brief The viewport's state kept between frames: the editor camera, a drag or a pan. */
struct ViewportState {
    std::optional<ViewCamera> camera;
    std::optional<ViewDrag> drag;
    std::optional<HandleDrag> handle;
    /** @brief The prefab dragged from the sidebar, until the button is released. */
    std::optional<std::string> placing;
    std::optional<engine::core::Vec2> pan_from;
    ViewportFrame frame;
};

} // namespace editor
