#include "action/ApplyCommand.hpp"

#include <cmath>
#include <variant>

#include "action/Overloaded.hpp"

namespace editor {

using engine::editor::CommandResult;
using engine::editor::EditorSession;

namespace {

CommandResult set_overlay(ViewSettings& view, const command::SetOverlay& overlay) {
    switch (overlay.overlay) {
    case Overlay::hitboxes:
        view.show_hitboxes = overlay.visible;
        break;
    case Overlay::names:
        view.show_names = overlay.visible;
        break;
    }
    return CommandResult::done();
}

CommandResult set_grid_size(ViewSettings& view, float size) {
    if (!std::isfinite(size) || size <= 0.F) {
        return CommandResult::refused("The grid size must be a positive number.");
    }
    view.grid_size = size;
    return CommandResult::done();
}

CommandResult play(Mode& mode) {
    if (mode == Mode::playing) {
        return CommandResult::refused("The game is already playing.");
    }
    mode = Mode::playing;
    return CommandResult::done();
}

CommandResult pause(Mode& mode) {
    if (mode != Mode::playing) {
        return CommandResult::refused("Only a playing game can be paused.");
    }
    mode = Mode::paused;
    return CommandResult::done();
}

CommandResult stop(Mode& mode) {
    if (mode == Mode::editing) {
        return CommandResult::refused("The game is not running.");
    }
    mode = Mode::editing;
    return CommandResult::done();
}

} // namespace

CommandResult apply_command(EditorSession& session, EditorState& state, const Command& command) {
    return std::visit(
        Overloaded{
            [&](const command::Select& select) { return session.select(select.id); },
            [&](const command::Undo&) { return session.undo(); },
            [&](const command::Redo&) { return session.redo(); },
            [&](const command::Save&) { return session.save(); },
            [&](const command::DiscardChanges&) { return session.discard_changes(); },
            [&](const command::CreateScene& scene) { return session.create_scene(scene.name); },
            [&](const command::OpenScene& scene) { return session.open_scene(scene.name); },
            [&](const command::DuplicateScene& scene) {
                return session.duplicate_scene(scene.from, scene.to);
            },
            [&](const command::RenameScene& scene) {
                return session.rename_scene(scene.from, scene.to);
            },
            [&](const command::DeleteScene& scene) { return session.delete_scene(scene.name); },
            [&](const command::SetStartScene& scene) {
                return session.set_start_scene(scene.name);
            },
            [&](const command::SetSnap& snap) {
                state.view.snap = snap.enabled;
                return CommandResult::done();
            },
            [&](const command::SetGridSize& grid) { return set_grid_size(state.view, grid.size); },
            [&](const command::SetOverlay& overlay) { return set_overlay(state.view, overlay); },
            [&](const command::SetTool& tool) {
                state.tool = tool.tool;
                return CommandResult::done();
            },
            [&](const command::ShowTab& tab) {
                state.bottom_tab = tab.tab;
                return CommandResult::done();
            },
            [&](const command::SetPrefabFilter& filter) {
                state.prefab_filter = filter.text;
                return CommandResult::done();
            },
            [&](const command::Play&) { return play(state.mode); },
            [&](const command::Pause&) { return pause(state.mode); },
            [&](const command::Stop&) { return stop(state.mode); },
        },
        command);
}

} // namespace editor
