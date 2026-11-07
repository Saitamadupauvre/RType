#pragma once

#include <optional>
#include <vector>

#include "engine/core/math/Vec2.hpp"
#include "engine/editor/SceneDocument.hpp"
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

/** @brief What the viewport draws this frame, read by the renderer through Clay's custom data. */
struct ViewportFrame {
    ViewCamera camera;
    std::vector<SceneItem> items;
    GridLines grid;
    std::optional<engine::editor::InstanceId> selection;
};

/** @brief The viewport's state kept between frames: the editor camera, a drag or a pan. */
struct ViewportState {
    std::optional<ViewCamera> camera;
    std::optional<ViewDrag> drag;
    std::optional<engine::core::Vec2> pan_from;
    ViewportFrame frame;
};

} // namespace editor
