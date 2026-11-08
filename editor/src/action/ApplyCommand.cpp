#include "action/ApplyCommand.hpp"

#include <cmath>
#include <filesystem>
#include <format>
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

CommandResult open_script(const FileOpener& open_file, const std::filesystem::path& file) {
    if (!open_file) {
        return CommandResult::refused("Opening files is not available here.");
    }
    if (!open_file(file)) {
        return CommandResult::refused(std::format("Cannot open {}.", file.string()));
    }
    return CommandResult::done();
}

CommandResult export_game(EditorState& state) {
    if (!state.export_dialog) {
        return CommandResult::refused("the export dialog is not open");
    }
    const ExportPlan& plan = state.export_dialog->plan;
    if (!plan.problems.empty()) {
        return CommandResult::refused(std::format("cannot export: {}", plan.problems.front()));
    }
    CommandResult result = run_export(plan);
    if (result.ok) {
        state.export_dialog.reset();
    }
    return result;
}

} // namespace

CommandResult apply_command(EditorSession& session, EditorState& state, EditorServices services,
                            const Command& command) {
    PlaySession& play = services.play;
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
            [&](const command::ReportInvalidInput& input) {
                return CommandResult::refused(input.message);
            },
            [&](const command::SetPrefabFilter& filter) {
                state.prefab_filter = filter.text;
                return CommandResult::done();
            },
            [&](const command::OpenScript& open) {
                return open_script(services.open_file, open.file);
            },
            [&](const command::Play&) { return play.play(session, state.mode); },
            [&](const command::Pause&) { return play.pause(state.mode); },
            [&](const command::Stop&) { return play.stop(state.mode); },
            [&](const command::OpenExport&) {
                const ExportSettings settings = default_export(session.game());
                state.export_dialog = ExportDialog{
                    .settings = settings,
                    .plan = plan_export(session.game(), services.runtime, settings)};
                return CommandResult::done();
            },
            [&](const command::SetExport& change) {
                if (!state.export_dialog) {
                    return CommandResult::refused("the export dialog is not open");
                }
                state.export_dialog->settings = change.settings;
                state.export_dialog->plan =
                    plan_export(session.game(), services.runtime, change.settings);
                return CommandResult::done();
            },
            [&](const command::CloseExport&) {
                state.export_dialog.reset();
                return CommandResult::done();
            },
            [&](const command::RunExport&) { return export_game(state); },
        },
        command);
}

} // namespace editor
