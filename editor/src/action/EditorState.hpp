#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "action/Action.hpp"

namespace editor {

enum class Mode : std::uint8_t { editing, playing, paused };

struct ViewSettings {
    bool snap = true;
    float grid_size = 16.F;
    bool show_hitboxes = false;
    bool show_names = true;

    bool operator==(const ViewSettings&) const = default;
};

enum class Severity : std::uint8_t { info, error };

struct ConsoleLine {
    Severity severity = Severity::info;
    std::string text;

    bool operator==(const ConsoleLine&) const = default;
};

struct EditorState {
    Mode mode = Mode::editing;
    Tool tool = Tool::select;
    BottomTab bottom_tab = BottomTab::console;
    ViewSettings view;
    std::vector<ConsoleLine> console;
    std::vector<std::string> prefabs;
    std::vector<std::string> scenes;
};

} // namespace editor
