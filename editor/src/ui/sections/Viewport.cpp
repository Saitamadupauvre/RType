#include "ui/sections/Viewport.hpp"

#include <clay.h>

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>
#include <vector>

#include "engine/core/ecs/FieldRegistry.hpp"
#include "ui/Theme.hpp"
#include "viewport/Handles.hpp"
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

bool start_handle(const Editor& editor, const std::vector<SceneItem>& items, Vec2 pointer,
                  const ViewCamera& camera, Clay_BoundingBox box, ViewportState& state) {
    const std::optional<engine::editor::InstanceId> selection = editor.session().selection();
    const auto item =
        selection ? std::ranges::find(items, *selection, &SceneItem::id) : items.end();
    if (item == items.end()) {
        return false;
    }
    const std::optional<Handle> handle = handle_at(*item, pointer, camera, box);
    if (!handle) {
        return false;
    }
    state.handle = HandleDrag{
        .handle = *handle, .item = *item, .scale = item->scale, .rotation = item->rotation};
    return true;
}

void set_field(Ui& ui, engine::editor::InstanceId id, const char* key, float value, float before) {
    if (value != before) {
        ui.emit(edit::SetField{.id = id, .key = key, .value = engine::core::FieldValue{value}});
    }
}

bool drag_handle(Ui& ui, const Editor& editor, Vec2 world, HandleDrag& dragged) {
    if (ui.input().primary_down) {
        if (dragged.handle == Handle::scale) {
            dragged.scale = scale_for(dragged.item, world);
        } else {
            dragged.rotation =
                rotation_for(dragged.item, world, editor.state().view.snap && !ui.input().alt);
        }
        return false;
    }
    const engine::editor::InstanceId id = dragged.item.id;
    if (dragged.handle == Handle::scale) {
        set_field(ui, id, "Scale.x", dragged.scale.x, dragged.item.scale.x);
        set_field(ui, id, "Scale.y", dragged.scale.y, dragged.item.scale.y);
    } else {
        set_field(ui, id, "Rotation.degrees", dragged.rotation, dragged.item.rotation);
    }
    return true;
}

bool pressed(const UiInput& input, Key key) {
    return std::ranges::find(input.keys_pressed, key) != input.keys_pressed.end();
}

void place(Ui& ui, const Editor& editor, Vec2 world, bool over, ViewportState& state,
           Placing& placing) {
    const UiInput& input = ui.input();
    const ViewSettings& view = editor.state().view;
    const Vec2 half{square_size / 2.F, square_size / 2.F};
    const Vec2 top_left = drop_position(world - half, view.grid_size, view.snap, input.alt);
    if (over) {
        state.frame.ghost = WorldBox{.top_left = top_left, .size = half * 2.F};
    }
    const bool drops = placing.armed ? input.primary_pressed : !input.primary_down;
    if (pressed(input, Key::escape)) {
        state.placing.reset();
    } else if (drops && (over || placing.armed)) {
        if (over) {
            ui.emit(edit::Place{.type = placing.type, .position = top_left});
        }
        state.placing.reset();
    } else if (drops) {
        placing.armed = true;
    }
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
    state.frame.cursor = over ? std::optional<Vec2>(world) : std::nullopt;
    if (state.placing) {
        place(ui, editor, world, over, state, *state.placing);
    } else if (state.handle) {
        if (drag_handle(ui, editor, world, *state.handle)) {
            state.handle.reset();
        }
    } else if (state.drag) {
        if (drag(ui, editor, world, *state.drag)) {
            state.drag.reset();
        }
    } else if (input.primary_pressed && over && editor.state().tool != Tool::pan &&
               !start_handle(editor, items, pointer_of(input), camera, box, state)) {
        press(ui, editor, items, world, state);
    }
}

void show_handle(const HandleDrag& dragged, SceneItem& item) {
    if (dragged.handle == Handle::rotate) {
        item.rotation = dragged.rotation;
        return;
    }
    const Vec2 base = unscaled_size(dragged.item);
    item.scale = dragged.scale;
    item.size = {base.x * std::abs(dragged.scale.x), base.y * std::abs(dragged.scale.y)};
}

void show_drag(const ViewportState& state, std::vector<SceneItem>& items) {
    if (state.drag) {
        const auto item = std::ranges::find(items, state.drag->id, &SceneItem::id);
        if (item != items.end()) {
            item->top_left = state.drag->preview;
        }
    }
    if (state.handle) {
        const auto item = std::ranges::find(items, state.handle->item.id, &SceneItem::id);
        if (item != items.end()) {
            show_handle(*state.handle, *item);
        }
    }
}

} // namespace

void viewport(Ui& ui, const Editor& editor) {
    ViewportState& state = ui.widgets().viewport();
    std::vector<SceneItem> items = scene_items(editor.session(), ui.platform().texture_size);
    const Clay_ElementData previous = Clay_GetElementData(viewport_id());
    state.frame.ghost.reset();
    state.frame.cursor.reset();
    if (previous.found) {
        ViewCamera& camera = state.camera
                                 ? *state.camera
                                 : state.camera.emplace(initial_camera(previous.boundingBox));
        interact(ui, editor, items, previous.boundingBox, state, camera);
        state.frame.grid = grid_lines(camera, previous.boundingBox, editor.state().view.grid_size);
    }
    show_drag(state, items);
    state.frame.camera = state.camera.value_or(ViewCamera{});
    state.frame.items = std::move(items);
    state.frame.selection = editor.session().selection();
    state.frame.game_frame = game_frame(editor.session());
    state.frame.show_names = editor.state().view.show_names;
    state.frame.show_hitboxes = editor.state().view.show_hitboxes;
    CLAY({.id = viewport_id(),
          .layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()}},
          .backgroundColor = color::viewport,
          .custom = {.customData = &state.frame}}) {}
}

} // namespace editor
