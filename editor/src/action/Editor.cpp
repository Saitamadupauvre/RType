#include "action/Editor.hpp"

#include <utility>
#include <variant>

#include "action/ApplyCommand.hpp"
#include "action/ApplyEdit.hpp"
#include "action/Overloaded.hpp"

namespace editor {

using engine::editor::CommandResult;
using engine::editor::EditorSession;

Editor::Editor(EditorSession session) : _session(std::move(session)) { refresh_lists(); }

void Editor::handle(const Action& action) {
    report(std::visit(Overloaded{
                          [&](const Edit& edit) { return apply(edit); },
                          [&](const Command& command) { return apply(command); },
                      },
                      action));
}

const EditorState& Editor::state() const noexcept { return _state; }

const EditorSession& Editor::session() const noexcept { return _session; }

CommandResult Editor::apply(const Edit& edit) {
    if (_state.mode != Mode::editing) {
        return CommandResult::refused("Stop the game to edit the scene.");
    }
    return apply_edit(_session, edit);
}

CommandResult Editor::apply(const Command& command) {
    CommandResult result = apply_command(_session, _state, command);
    refresh_lists();
    return result;
}

void Editor::refresh_lists() {
    _state.prefabs = _session.prefabs();
    _state.scenes = _session.scenes();
}

void Editor::report(const CommandResult& result) {
    if (!result.ok) {
        _state.console.push_back({.severity = Severity::error, .text = result.message});
        return;
    }
    if (!result.message.empty()) {
        _state.console.push_back({.severity = Severity::info, .text = result.message});
    }
}

} // namespace editor
