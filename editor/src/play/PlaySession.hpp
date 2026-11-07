#pragma once

#include <filesystem>
#include <memory>
#include <vector>

#include "action/EditorState.hpp"
#include "engine/editor/CommandResult.hpp"
#include "engine/editor/EditorSession.hpp"
#include "play/GameLauncher.hpp"

namespace editor {

/**
 * @brief Plays the open scene outside the editor and follows the game until it ends.
 *
 * Play writes the scene document, unsaved changes included, to a file in a scratch folder and
 * starts the game from it; the scene files of the game are never touched. The document itself
 * is never changed by playing, so Stop has nothing to restore.
 */
class PlaySession {
public:
    /** @param scratch Folder for the played scene file; created when needed. */
    explicit PlaySession(std::filesystem::path scratch);
    ~PlaySession();
    PlaySession(const PlaySession&) = delete;
    PlaySession& operator=(const PlaySession&) = delete;
    PlaySession(PlaySession&&) noexcept;
    PlaySession& operator=(PlaySession&&) noexcept;

    void attach(std::unique_ptr<GameLauncher> launcher);

    /** @brief Starts the open scene, or resumes a paused game. */
    [[nodiscard]] engine::editor::CommandResult play(const engine::editor::EditorSession& session,
                                                     Mode& mode);
    [[nodiscard]] engine::editor::CommandResult pause(Mode& mode);
    [[nodiscard]] engine::editor::CommandResult stop(Mode& mode);

    /**
     * @brief Collects what the game printed and notices when it ended on its own, which brings
     * the editor back to Mode::editing.
     * @return The lines to show in the console.
     */
    [[nodiscard]] std::vector<ConsoleLine> update(Mode& mode);

private:
    void finish();

    std::filesystem::path _scratch;
    std::unique_ptr<GameLauncher> _launcher;
    std::unique_ptr<GameRun> _run;
};

} // namespace editor
