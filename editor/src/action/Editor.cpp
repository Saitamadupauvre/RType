#include "action/Editor.hpp"

#include <utility>
#include <variant>

#include "action/ApplyCommand.hpp"
#include "action/ApplyEdit.hpp"
#include "action/Overloaded.hpp"

namespace editor {

using engine::editor::CommandResult;
using engine::editor::EditorSession;

Editor::Editor(EditorSession session) : session_(std::move(session)) { refresh_lists(); }

void Editor::handle(const Action& action) {
    report(std::visit(Overloaded{
                          [&](const Edit& edit) { return apply(edit); },
                          [&](const Command& command) { return apply(command); },
                      },
                      action));
}

const EditorState& Editor::state() const noexcept { return state_; }

const EditorSession& Editor::session() const noexcept { return session_; }

CommandResult Editor::apply(const Edit& edit) {
    if (state_.mode != Mode::editing) {
        return CommandResult::refused("Stop the game to edit the scene.");
    }
    return apply_edit(session_, edit);
}

CommandResult Editor::apply(const Command& command) {
    CommandResult result = apply_command(session_, state_, command);
    refresh_lists();
    return result;
}

void Editor::refresh_lists() {
    state_.prefabs = session_.prefabs();
    state_.scenes = session_.scenes();
}

void Editor::report(const CommandResult& result) {
    if (!result.ok) {
        state_.console.push_back({.severity = Severity::error, .text = result.message});
        return;
    }
    if (!result.message.empty()) {
        state_.console.push_back({.severity = Severity::info, .text = result.message});
    }
}

} // namespace editor
