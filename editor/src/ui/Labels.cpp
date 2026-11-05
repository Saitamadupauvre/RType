#include "ui/Labels.hpp"

namespace editor {

std::string_view instance_label(const engine::script::Instance& instance) {
    return instance.name.empty() ? instance.type : instance.name;
}

std::string_view mode_label(Mode mode) {
    switch (mode) {
    case Mode::editing:
        return "edit mode";
    case Mode::playing:
        return "playing";
    case Mode::paused:
        return "paused";
    }
    return "";
}

std::string scene_title(const engine::editor::EditorSession& session) {
    const auto& name = session.open_scene_name();
    if (!name) {
        return "no scene open";
    }
    return session.unsaved() ? *name + " •" : *name;
}

} // namespace editor
