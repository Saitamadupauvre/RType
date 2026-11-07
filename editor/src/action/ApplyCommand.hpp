#pragma once

#include "action/Action.hpp"
#include "action/EditorState.hpp"
#include "engine/editor/CommandResult.hpp"
#include "engine/editor/EditorSession.hpp"

namespace editor {

/**
 * @brief Applies a command to the session or to the editor state; does not check the mode.
 */
engine::editor::CommandResult apply_command(engine::editor::EditorSession& session,
                                            EditorState& state, const Command& command);

} // namespace editor
