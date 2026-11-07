#include "ui/sections/Viewport.hpp"

#include <clay.h>

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include "ui/Theme.hpp"
#include "viewport/SceneItems.hpp"
#include "viewport/Snap.hpp"
#include "viewport/ViewCamera.hpp"
#include "viewport/ViewportState.hpp"

namespace editor {

namespace {

using engine::core::Vec2;

Clay_ElementId viewport_id() { return CLAY_ID("Viewport"); }

Vec2 pointer_of(const UiInput& input) { return {input.pointer.x, input.pointer.y}; }

void pan(const UiInput& input, Tool tool, bool over, ViewportState& state, ViewCamera& camera) {
    const bool held = input.middle_down || (tool == Tool::pan && input.primary_down);
    if (!held) {
        state.pan_from.reset();
        return;
    }
    const Vec2 pointer = pointer_of(input);
    if (state.pan_from) {
        camera = panned(camera, pointer - *state.pan_from);
        state.pan_from = pointer;
    } else if (over) {
        state.pan_from = pointer;
    }
}

void press(Ui& ui, const Editor& editor, const std::vector<SceneItem>& items, Vec2 world,
           ViewportState& state) {
    const std::optional<engine::editor::InstanceId> hit = item_at(items, world);
    if (hit != editor.session().selection()) {
        ui.emit(command::Select{.id = hit});
    }
    if (!hit || editor.state().tool != Tool::move) {
        return;
    }
    const auto item = std::ranges::find(items, *hit, &SceneItem::id);
    state.drag = ViewDrag{.id = *hit,
                          .grab = world - item->top_left,
                          .start = item->top_left,
                          .preview = item->top_left};
}

bool drag(Ui& ui, const Editor& editor, Vec2 world, ViewDrag& dragged) {
    const ViewSettings& view = editor.state().view;
    if (ui.input().primary_down) {
        const Vec2 target = world - dragged.grab;
        dragged.preview = target == dragged.start
                              ? dragged.start
                              : drop_position(target, view.grid_size, view.snap, ui.input().alt);
        return false;
    }
    if (dragged.preview != dragged.start) {
        ui.emit(edit::Move{.id = dragged.id, .position = dragged.preview});
    }
    return true;
}

void interact(Ui& ui, const Editor& editor, const std::vector<SceneItem>& items,
              Clay_BoundingBox box, ViewportState& state, ViewCamera& camera) {
    const UiInput& input = ui.input();
    const bool over = Ui::hovered(viewport_id());
    if (over && input.wheel.y != 0.F) {
        camera = zoomed(camera, box, pointer_of(input), input.wheel.y);
    }
    pan(input, editor.state().tool, over, state, camera);
    const Vec2 world = to_world(camera, box, pointer_of(input));
    if (state.drag) {
        if (drag(ui, editor, world, *state.drag)) {
            state.drag.reset();
        }
    } else if (input.primary_pressed && over && editor.state().tool != Tool::pan) {
        press(ui, editor, items, world, state);
    }
}

void show_drag(const ViewportState& state, std::vector<SceneItem>& items) {
    if (!state.drag) {
        return;
    }
    const auto item = std::ranges::find(items, state.drag->id, &SceneItem::id);
    if (item != items.end()) {
        item->top_left = state.drag->preview;
    }
}

} // namespace

void viewport(Ui& ui, const Editor& editor) {
    ViewportState& state = ui.widgets().viewport();
    std::vector<SceneItem> items = scene_items(editor.session(), ui.platform().texture_size);
    const Clay_ElementData previous = Clay_GetElementData(viewport_id());
    if (previous.found) {
        ViewCamera& camera = state.camera ? *state.camera
                                          : state.camera.emplace(initial_camera(previous.boundingBox));
        interact(ui, editor, items, previous.boundingBox, state, camera);
        state.frame.grid = grid_lines(camera, previous.boundingBox, editor.state().view.grid_size);
    }
    show_drag(state, items);
    state.frame.camera = state.camera.value_or(ViewCamera{});
    state.frame.items = std::move(items);
    state.frame.selection = editor.session().selection();
    CLAY({.id = viewport_id(),
          .layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()}},
          .backgroundColor = color::viewport,
          .custom = {.customData = &state.frame}}) {}
}

} // namespace editor
