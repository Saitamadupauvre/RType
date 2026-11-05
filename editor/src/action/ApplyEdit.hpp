#pragma once

#include "action/Action.hpp"
#include "engine/editor/CommandResult.hpp"
#include "engine/editor/EditorSession.hpp"

namespace editor {

engine::editor::CommandResult apply_edit(engine::editor::EditorSession& session, const Edit& edit);

} // namespace editor
