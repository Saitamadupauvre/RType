#include "action/ActionText.hpp"

#include <format>
#include <optional>
#include <string_view>
#include <variant>

#include "action/Overloaded.hpp"
#include "ui/FieldText.hpp"

namespace editor {

namespace {

std::string id_text(InstanceId id) { return std::to_string(id.value); }

std::string point_text(engine::core::Vec2 point) {
    return std::format("({:g}, {:g})", point.x, point.y);
}

std::string_view tool_name(Tool tool) {
    switch (tool) {
    case Tool::select:
        return "select";
    case Tool::move:
        return "move";
    case Tool::pan:
        return "pan";
    }
    return "";
}

std::string_view tab_name(BottomTab tab) {
    switch (tab) {
    case BottomTab::console:
        return "console";
    case BottomTab::problems:
        return "problems";
    case BottomTab::assets:
        return "assets";
    }
    return "";
}

std::string_view overlay_name(Overlay overlay) {
    return overlay == Overlay::hitboxes ? "hitboxes" : "names";
}

std::string joined_modes(const ExportModes& modes) {
    std::string text;
    for (const std::string& name : mode_names(modes)) {
        text += text.empty() ? name : "," + name;
    }
    return text.empty() ? std::string("none") : text;
}

std::string edit_text(const Edit& edit) {
    return std::visit(
        Overloaded{
            [](const edit::Place& place) {
                return std::format("edit::Place type={} position={}", place.type,
                                   point_text(place.position));
            },
            [](const edit::Move& move) {
                return std::format("edit::Move id={} position={}", id_text(move.id),
                                   point_text(move.position));
            },
            [](const edit::Remove& remove) {
                return std::format("edit::Remove id={}", id_text(remove.id));
            },
            [](const edit::Rename& rename) {
                return std::format("edit::Rename id={} name={}", id_text(rename.id), rename.name);
            },
            [](const edit::SetField& set) {
                return std::format("edit::SetField id={} key={} value={}", id_text(set.id), set.key,
                                   describe(set.value));
            },
            [](const edit::ResetField& reset) {
                return std::format("edit::ResetField id={} key={}", id_text(reset.id), reset.key);
            },
            [](const edit::ResetAll& reset) {
                return std::format("edit::ResetAll id={}", id_text(reset.id));
            },
            [](const edit::ChangeType& change) {
                return std::format("edit::ChangeType id={} type={}", id_text(change.id),
                                   change.type);
            },
            [](const edit::Duplicate& duplicate) {
                return std::format("edit::Duplicate id={} offset={}", id_text(duplicate.id),
                                   point_text(duplicate.offset));
            },
        },
        edit);
}

std::string scene_text(const Command& command) {
    return std::visit(
        Overloaded{
            [](const command::CreateScene& scene) {
                return std::format("command::CreateScene name={}", scene.name);
            },
            [](const command::OpenScene& scene) {
                return std::format("command::OpenScene name={}", scene.name);
            },
            [](const command::CreatePrefab& prefab) {
                return std::format("command::CreatePrefab name={}", prefab.name);
            },
            [](const command::DuplicateScene& scene) {
                return std::format("command::DuplicateScene from={} to={}", scene.from, scene.to);
            },
            [](const command::RenameScene& scene) {
                return std::format("command::RenameScene from={} to={}", scene.from, scene.to);
            },
            [](const command::DeleteScene& scene) {
                return std::format("command::DeleteScene name={}", scene.name);
            },
            [](const command::SetStartScene& scene) {
                return std::format("command::SetStartScene name={}", scene.name);
            },
            [](const auto&) { return std::string{}; },
        },
        command);
}

std::string command_text(const Command& command) {
    if (std::string scene = scene_text(command); !scene.empty()) {
        return scene;
    }
    return std::visit(
        Overloaded{
            [](const command::Select& select) {
                return std::format("command::Select id={}",
                                   select.id ? id_text(*select.id) : std::string("none"));
            },
            [](const command::Undo&) { return std::string("command::Undo"); },
            [](const command::Redo&) { return std::string("command::Redo"); },
            [](const command::Save&) { return std::string("command::Save"); },
            [](const command::DiscardChanges&) { return std::string("command::DiscardChanges"); },
            [](const command::SetSnap& snap) {
                return std::format("command::SetSnap enabled={}", snap.enabled);
            },
            [](const command::SetGridSize& grid) {
                return std::format("command::SetGridSize size={:g}", grid.size);
            },
            [](const command::SetOverlay& overlay) {
                return std::format("command::SetOverlay overlay={} visible={}",
                                   overlay_name(overlay.overlay), overlay.visible);
            },
            [](const command::SetTool& tool) {
                return std::format("command::SetTool tool={}", tool_name(tool.tool));
            },
            [](const command::ShowTab& tab) {
                return std::format("command::ShowTab tab={}", tab_name(tab.tab));
            },
            [](const command::ReportInvalidInput& input) {
                return std::format("command::ReportInvalidInput message={}", input.message);
            },
            [](const command::SetPrefabFilter& filter) {
                return std::format("command::SetPrefabFilter text={}", filter.text);
            },
            [](const command::OpenScript& open) {
                return std::format("command::OpenScript file={}", open.file.generic_string());
            },
            [](const command::RevealGameFolder&) {
                return std::string("command::RevealGameFolder");
            },
            [](const command::OpenSettings&) { return std::string("command::OpenSettings"); },
            [](const command::ReloadSettings&) { return std::string("command::ReloadSettings"); },
            [](const command::ReloadScripts&) { return std::string("command::ReloadScripts"); },
            [](const command::Play&) { return std::string("command::Play"); },
            [](const command::Pause&) { return std::string("command::Pause"); },
            [](const command::Stop&) { return std::string("command::Stop"); },
            [](const command::OpenExport&) { return std::string("command::OpenExport"); },
            [](const command::SetExport& change) {
                return std::format("command::SetExport name={} output={} modes={}",
                                   change.settings.name, change.settings.output.generic_string(),
                                   joined_modes(change.settings.modes));
            },
            [](const command::CloseExport&) { return std::string("command::CloseExport"); },
            [](const command::OpenNameDialog& open) {
                return std::format("command::OpenNameDialog item={}",
                                   open.item == NewItem::prefab ? "prefab" : "scene");
            },
            [](const command::SetNameDialogText& text) {
                return std::format("command::SetNameDialogText name={}", text.name);
            },
            [](const command::CloseNameDialog&) { return std::string("command::CloseNameDialog"); },
            [](const command::RunExport&) { return std::string("command::RunExport"); },
            [](const auto&) { return std::string{}; },
        },
        command);
}

} // namespace

std::string describe_action(const Action& action) {
    return std::visit(Overloaded{
                          [](const Edit& edit) { return edit_text(edit); },
                          [](const Command& command) { return command_text(command); },
                      },
                      action);
}

} // namespace editor
