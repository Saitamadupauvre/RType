#include <iostream>

#include "action/OpenEditor.hpp"
#include "cli/Cli.hpp"
#include "raylib/EditorWindow.hpp"

int main(int argc, char** argv) {
    const auto result = editor::parse_cli({argv, argv + argc});
    if (!result.config.has_value()) {
        (result.exit_code == 0 ? std::cout : std::cerr) << result.message;
        return result.exit_code;
    }
    auto editor = editor::open_editor(result.config->game);
    if (!editor) {
        std::cerr << "editor: cannot open " << result.config->game.string() << "\n";
        return 84;
    }
    return editor::run_editor_window(*editor) ? 0 : 84;
}
