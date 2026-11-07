#include "ui/sections/Sidebar.hpp"

#include <clay.h>

#include <cstdint>
#include <string>
#include <vector>

#include "ui/Labels.hpp"
#include "ui/SmoothScroll.hpp"
#include "ui/Text.hpp"
#include "ui/sections/PanelHeader.hpp"
#include "ui/widgets/FieldEvent.hpp"
#include "ui/widgets/ListRow.hpp"
#include "ui/widgets/TextField.hpp"

namespace editor {

namespace {

template <typename Pinned, typename Content>
void panel_section(Ui& ui, Clay_ElementId id, Clay_String title, Pinned pinned, Content content) {
    CLAY({.layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()},
                     .layoutDirection = CLAY_TOP_TO_BOTTOM}}) {
        panel_header(ui.theme(), title);
        pinned();
        CLAY({.id = id,
              .layout = {.sizing = {.width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW()},
                         .layoutDirection = CLAY_TOP_TO_BOTTOM},
              .clip = scroll_clip(ui.widgets().scroll(), id)}) {
            content();
        }
    }
}

template <typename Content>
void panel_section(Ui& ui, Clay_ElementId id, Clay_String title, Content content) {
    panel_section(ui, id, title, [] {}, content);
}

void prefab_filter(Ui& ui, const std::string& filter) {
    const Theme& theme = ui.theme();
    CLAY({.layout = {.sizing = {.width = CLAY_SIZING_GROW()},
                     .padding = CLAY_PADDING_ALL(theme.px16(6.F))}}) {
        const TextResult result = text_field(ui, CLAY_ID("PrefabFilter"), filter,
                                             {.placeholder = "Filter prefabs"});
        if (result.event != FieldEvent::none && result.value != filter) {
            ui.emit(command::SetPrefabFilter{.text = result.value});
        }
    }
}

void prefabs(Ui& ui, const EditorState& state) {
    const Theme& theme = ui.theme();
    if (state.prefabs.empty()) {
        empty_hint(theme, CLAY_STRING("No prefabs declared"));
        return;
    }
    bool any = false;
    for (std::uint32_t index = 0; index < state.prefabs.size(); ++index) {
        const std::string& type = state.prefabs[index];
        if (!matches_filter(type, state.prefab_filter)) {
            continue;
        }
        any = true;
        if (list_row(ui, CLAY_IDI("PrefabRow", index), {.label = view_of(type)})) {
            ui.widgets().viewport().placing = type;
        }
    }
    if (!any) {
        empty_hint(theme, CLAY_STRING("No prefab matches"));
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
        panel_section(
            ui, CLAY_ID("Prefabs"), CLAY_STRING("PREFABS"),
            [&] { prefab_filter(ui, editor.state().prefab_filter); },
            [&] { prefabs(ui, editor.state()); });
        panel_section(ui, CLAY_ID("Scenes"), CLAY_STRING("SCENES"), [&] { scenes(ui, editor); });
        panel_section(ui, CLAY_ID("Instances"), CLAY_STRING("INSTANCES"),
                      [&] { instances(ui, editor.session()); });
    }
}

} // namespace editor
