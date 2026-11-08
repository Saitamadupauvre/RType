#include <filesystem>
#include <iostream>
#include <memory>

#include "action/OpenEditor.hpp"
#include "cli/Cli.hpp"
#include "play/ProcessLauncher.hpp"
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
    const std::filesystem::path runtime =
        result.config->runtime.empty()
            ? std::filesystem::path(argv[0]).parent_path() / "r-type_client"
            : result.config->runtime;
    editor->attach_runtime(runtime.parent_path());
    editor->attach_launcher(std::make_unique<editor::ProcessLauncher>(runtime));
    return editor::run_editor_window(*editor) ? 0 : 84;
}
