#include "action/Editor.hpp"

#include <filesystem>
#include <system_error>
#include <utility>
#include <variant>

#include "action/ApplyCommand.hpp"
#include "action/ApplyEdit.hpp"
#include "action/Overloaded.hpp"

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
    _state.console.push_back(std::move(line));
}

void Editor::update() {
    for (ConsoleLine& line : _play.update(_state.mode)) {
        log(std::move(line));
    }
}

void Editor::handle(const Action& action) {
    report(std::visit(Overloaded{
                          [&](const Edit& edit) { return apply(edit); },
                          [&](const Command& command) { return apply(command); },
                      },
                      action));
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
    CommandResult result = apply_command(_session, _state, {.play = _play, .open_file = _open_file}, command);
    refresh_lists();
    return result;
}

void Editor::refresh_inspector() { _state.inspector = inspect_selection(_session); }

void Editor::refresh_lists() {
    _state.prefabs = _session.prefabs();
    _state.scenes = _session.scenes();
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
