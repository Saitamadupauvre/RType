#include "ui/sections/Sidebar.hpp"

#include <clay.h>

#include <cstdint>
#include <string>
#include <vector>

#include "ui/Labels.hpp"
#include "ui/Text.hpp"
#include "ui/sections/PanelHeader.hpp"
#include "ui/widgets/ListRow.hpp"

namespace editor {

namespace {

template <typename Content>
void panel_section(const Theme& theme, Clay_ElementId id, Clay_String title, Content content) {
    CLAY({.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM}}) {
        panel_header(theme, title);
        CLAY({.id = id,
              .layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()},
                         .layoutDirection = CLAY_TOP_TO_BOTTOM},
              .clip = {.vertical = true, .childOffset = Clay_GetScrollOffset()}}) {
            content();
        }
    }
}

void prefabs(const Theme& theme, const std::vector<std::string>& types) {
    if (types.empty()) {
        empty_hint(theme, CLAY_STRING("No prefabs declared"));
    }
    for (const std::string& type : types) {
        list_item(theme, {.label = view_of(type)});
    }
}

void scenes(Ui& ui, const Editor& editor) {
    const auto& session = editor.session();
    const auto& names = editor.state().scenes;
    if (names.empty()) {
        empty_hint(ui.theme(), CLAY_STRING("No scenes"));
    }
    for (std::uint32_t index = 0; index < names.size(); ++index) {
        const std::string& name = names[index];
        const bool open = session.open_scene_name() == name;
        const bool start = session.start_scene() == name;
        if (list_row(ui, CLAY_IDI("SceneRow", index),
                     {.label = view_of(name),
                      .detail = start ? CLAY_STRING("start") : Clay_String{},
                      .selected = open}) &&
            !open) {
            ui.emit(command::OpenScene{.name = name});
        }
    }
}

void instances(Ui& ui, const engine::editor::EditorSession& session) {
    if (!session.open_scene_name()) {
        empty_hint(ui.theme(), CLAY_STRING("No scene open"));
        return;
    }
    const auto& entries = session.document().instances;
    if (entries.empty()) {
        empty_hint(ui.theme(), CLAY_STRING("No instances"));
    }
    for (std::uint32_t index = 0; index < entries.size(); ++index) {
        const auto& entry = entries[index];
        const bool named = !entry.instance.name.empty();
        if (list_row(ui, CLAY_IDI("InstanceRow", index),
                     {.label = view_of(instance_label(entry.instance)),
                      .detail = named ? view_of(entry.instance.type) : Clay_String{},
                      .selected = session.selection() == entry.id})) {
            ui.emit(command::Select{.id = entry.id});
        }
    }
}

} // namespace

void sidebar(Ui& ui, const Editor& editor) {
    const Theme& theme = ui.theme();
    CLAY({.id = CLAY_ID("Sidebar"),
          .layout = {.sizing = {.width = CLAY_SIZING_FIXED(theme.px(size::sidebar)),
                                .height = CLAY_SIZING_GROW()},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM},
          .backgroundColor = color::panel,
          .border = {.color = color::border, .width = {.right = theme.px16(1.F)}}}) {
        panel_section(theme, CLAY_ID("Prefabs"), CLAY_STRING("PREFABS"),
                      [&] { prefabs(theme, editor.state().prefabs); });
        panel_section(theme, CLAY_ID("Scenes"), CLAY_STRING("SCENES"), [&] { scenes(ui, editor); });
        panel_section(theme, CLAY_ID("Instances"), CLAY_STRING("INSTANCES"),
                      [&] { instances(ui, editor.session()); });
    }
}

} // namespace editor
