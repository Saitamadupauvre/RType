#include "ui/Labels.hpp"

#include <algorithm>
#include <cctype>

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

bool matches_filter(std::string_view label, std::string_view filter) {
    const auto lower = [](char letter) { return std::tolower(static_cast<unsigned char>(letter)); };
    return filter.empty() || !std::ranges::search(label, filter, {}, lower, lower).empty();
}

} // namespace editor
