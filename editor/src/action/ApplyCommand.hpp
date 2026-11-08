#pragma once

#include "action/Action.hpp"
#include "action/EditorState.hpp"
#include "engine/editor/CommandResult.hpp"
#include "engine/editor/EditorSession.hpp"
#include "play/PlaySession.hpp"

namespace editor {

/**
 * @brief Applies a command to the session, the editor state or the played game; does not check
 * the mode.
 */
engine::editor::CommandResult apply_command(engine::editor::EditorSession& session,
                                            EditorState& state, PlaySession& play,
                                            const Command& command);

} // namespace editor
