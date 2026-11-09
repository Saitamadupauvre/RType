#pragma once

#include <filesystem>

#include "action/Action.hpp"
#include "action/EditorState.hpp"
#include "engine/editor/CommandResult.hpp"
#include "engine/editor/EditorSession.hpp"
#include "open/FileOpener.hpp"
#include "play/PlaySession.hpp"

namespace editor {

/** @brief What commands reach besides the session and the state. */
struct EditorServices {
    PlaySession& play;
    /** Opens files with the system's program; null when the editor has no display. */
    FileOpener* open_file;
    /** Folder holding the runtime binaries an export copies; empty when unknown. */
    const std::filesystem::path& runtime;
};

/**
 * @brief Applies a command to the session, the editor state or the played game; does not check
 * the mode.
 */
engine::editor::CommandResult apply_command(engine::editor::EditorSession& session,
                                            EditorState& state, EditorServices services,
                                            const Command& command);

} // namespace editor
