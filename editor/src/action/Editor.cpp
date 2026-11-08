#include "action/Editor.hpp"

#include <filesystem>
#include <format>
#include <system_error>
#include <utility>
#include <variant>

#include "action/ActionText.hpp"
#include "action/ApplyCommand.hpp"
#include "action/ApplyEdit.hpp"
#include "action/Overloaded.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/script/GameFolder.hpp"
#include "text/ScriptLocation.hpp"

namespace editor {

using engine::editor::CommandResult;
using engine::editor::EditorSession;

namespace {

bool allowed_while_running(const Command& command) {
    return std::visit([](const auto& known) { return known.while_running; }, command);
}

CommandResult stop_first() {
    return CommandResult::refused("Stop the game to change the project.");
}

} // namespace

Editor::Editor(EditorSession session, std::filesystem::path scratch)
    : _session(std::move(session)), _play(std::move(scratch)) {
    refresh_lists();
    refresh_inspector();
}

std::filesystem::path Editor::default_scratch() {
    std::error_code error;
    const std::filesystem::path temporary = std::filesystem::temp_directory_path(error);
    return (error ? std::filesystem::path(".") : temporary) / "engine-editor";
}

void Editor::attach_launcher(std::unique_ptr<GameLauncher> launcher) {
    _play.attach(std::move(launcher));
}

void Editor::attach_file_opener(FileOpener opener) { _open_file = std::move(opener); }

void Editor::attach_clock(std::function<std::string()> clock) { _clock = std::move(clock); }

void Editor::log(ConsoleLine line) {
    if (_clock) {
        line.time = _clock();
    }
    if (const auto location = script_location(line.text)) {
        const auto file = engine::script::scripts_folder(_session.game()) / location->file;
        std::error_code error;
        if (std::filesystem::is_regular_file(file, error)) {
            line.script = file;
        }
    }
    _state.console.push_back(std::move(line));
}

void Editor::attach_runtime(std::filesystem::path folder) { _runtime = std::move(folder); }

void Editor::apply_settings(const LoadedSettings& loaded) {
    namespace logging = engine::core::log;
    _state.view = loaded.settings.view;
    _state.ui_scale = loaded.settings.ui_scale;
    _state.scroll_speed = loaded.settings.scroll_speed;
    _state.settings_file = loaded.file;
    for (const ConsoleLine& problem : loaded.problems) {
        if (problem.severity == Severity::error) {
            logging::error("editor: {}", problem.text);
        } else {
            logging::warn("editor: {}", problem.text);
        }
        log(problem);
    }
}

void Editor::update() {
    for (ConsoleLine& line : _play.update(_state.mode)) {
        log(std::move(line));
    }
}

void Editor::handle(const Action& action) {
    namespace logging = engine::core::log;
    logging::debug("editor: {}", describe_action(action));
    const CommandResult result =
        std::visit(Overloaded{
                       [&](const Edit& edit) { return apply(edit); },
                       [&](const Command& command) { return apply(command); },
                   },
                   action);
    if (!result.ok) {
        logging::info("editor: refused {}: {}", describe_action(action), result.message);
    }
    report(result);
    refresh_inspector();
}

const EditorState& Editor::state() const noexcept { return _state; }

const EditorSession& Editor::session() const noexcept { return _session; }

CommandResult Editor::apply(const Edit& edit) {
    if (_state.mode != Mode::editing) {
        return stop_first();
    }
    return apply_edit(_session, edit);
}

CommandResult Editor::apply(const Command& command) {
    if (_state.mode != Mode::editing && !allowed_while_running(command)) {
        return stop_first();
    }
    if (std::holds_alternative<command::ReloadSettings>(command)) {
        return reload_settings();
    }
    CommandResult result = apply_command(
        _session, _state, {.play = _play, .open_file = _open_file, .runtime = _runtime}, command);
    refresh_lists();
    return result;
}

CommandResult Editor::reload_settings() {
    if (_state.settings_file.empty()) {
        return CommandResult::refused(
            "No settings folder: neither XDG_CONFIG_HOME nor HOME is set.");
    }
    apply_settings(load_settings(_state.settings_file));
    return CommandResult::done(
        std::format("settings reloaded from {}", _state.settings_file.generic_string()));
}

void Editor::refresh_inspector() { _state.inspector = inspect_selection(_session); }

void Editor::refresh_lists() {
    _state.prefabs = _session.prefabs();
    _state.scenes = _session.scenes();
    _state.assets = list_assets(_session.game());
}

void Editor::report(const CommandResult& result) {
    if (!result.ok) {
        log({.severity = Severity::error, .text = result.message});
        return;
    }
    if (!result.message.empty()) {
        log({.severity = Severity::info, .text = result.message});
    }
}

} // namespace editor
