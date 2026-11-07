#pragma once

#include "action/Action.hpp"
#include "action/EditorState.hpp"
#include "engine/editor/CommandResult.hpp"
#include "engine/editor/EditorSession.hpp"

namespace editor {

/**
 * @brief Owns the session and the editor state, and applies every Action.
 *
 * The UI reads state() and session() and never changes them: it emits actions, which the loop
 * hands to handle() after the frame is drawn.
 */
class Editor {
public:
    explicit Editor(engine::editor::EditorSession session);

    /**
     * @brief Applies one action. A refused action changes nothing and adds an error line to the
     * console; a successful one with a note adds an info line.
     *
     * Edits, and commands whose `while_running` is false, are refused outside Mode::editing.
     */
    void handle(const Action& action);

    [[nodiscard]] const EditorState& state() const noexcept;
    [[nodiscard]] const engine::editor::EditorSession& session() const noexcept;

private:
    [[nodiscard]] engine::editor::CommandResult apply(const Edit& edit);
    [[nodiscard]] engine::editor::CommandResult apply(const Command& command);
    void refresh_lists();
    void refresh_inspector();
    void report(const engine::editor::CommandResult& result);

    engine::editor::EditorSession _session;
    EditorState _state;
};

} // namespace editor
