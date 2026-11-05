#pragma once

#include "action/Action.hpp"
#include "action/EditorState.hpp"
#include "engine/editor/CommandResult.hpp"
#include "engine/editor/EditorSession.hpp"

namespace editor {

engine::editor::CommandResult apply_command(engine::editor::EditorSession& session,
                                            EditorState& state, const Command& command);

} // namespace editor
