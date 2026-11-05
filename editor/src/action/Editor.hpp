#pragma once

#include "action/Action.hpp"
#include "action/EditorState.hpp"
#include "engine/editor/CommandResult.hpp"
#include "engine/editor/EditorSession.hpp"

namespace editor {

class Editor {
public:
    explicit Editor(engine::editor::EditorSession session);

    void handle(const Action& action);

    [[nodiscard]] const EditorState& state() const noexcept;
    [[nodiscard]] const engine::editor::EditorSession& session() const noexcept;

private:
    [[nodiscard]] engine::editor::CommandResult apply(const Edit& edit);
    [[nodiscard]] engine::editor::CommandResult apply(const Command& command);
    void refresh_lists();
    void report(const engine::editor::CommandResult& result);

    engine::editor::EditorSession _session;
    EditorState _state;
};

} // namespace editor
