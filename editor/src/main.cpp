#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "action/OpenEditor.hpp"
#include "engine/core/log/Log.hpp"
#include "cli/Cli.hpp"
#include "play/ProcessLauncher.hpp"
#include "raylib/EditorWindow.hpp"
#include "settings/Settings.hpp"

int main(int argc, char** argv) {
    const auto result = editor::parse_cli({argv, argv + argc});
    if (!result.config.has_value()) {
        (result.exit_code == 0 ? std::cout : std::cerr) << result.message;
        return result.exit_code;
    }
    engine::core::log::set_level(result.config->log_level);
    auto editor = editor::open_editor(result.config->game);
    if (!editor) {
        std::cerr << "editor: cannot open " << result.config->game.string() << "\n";
        return 84;
    }
    const std::filesystem::path runtime =
        result.config->runtime.empty()
            ? std::filesystem::path(argv[0]).parent_path() / "r-type_client"
            : result.config->runtime;
    const auto settings_file = editor::settings_path([](std::string_view name) {
        const std::string key(name);
        const char* value = std::getenv(key.c_str());
        return value ? std::optional<std::string>(value) : std::nullopt;
    });
    const editor::LoadedSettings settings =
        settings_file ? editor::load_settings(*settings_file) : editor::LoadedSettings{};
    if (settings_file) {
        engine::core::log::info("editor: settings {}", settings_file->generic_string());
    }
    editor->apply_settings(settings);
    editor->attach_runtime(runtime.parent_path());
    editor->attach_launcher(std::make_unique<editor::ProcessLauncher>(runtime));
    return editor::run_editor_window(*editor) ? 0 : 84;
}
