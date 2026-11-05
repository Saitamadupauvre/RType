#include <iostream>
#include <span>

#include "action/OpenEditor.hpp"
#include "raylib/EditorWindow.hpp"

int main(int argc, char** argv) {
    const std::span arguments(argv, static_cast<std::size_t>(argc));
    if (arguments.size() != 2) {
        std::cerr << "usage: editor <game folder>\n";
        return 84;
    }
    auto editor = editor::open_editor(arguments[1]);
    if (!editor) {
        std::cerr << "editor: " << arguments[1] << " is not a game folder\n";
        return 84;
    }
    return editor::run_editor_window(*editor) ? 0 : 84;
}
