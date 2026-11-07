#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <string>

#include "action/Action.hpp"
#include "action/ApplyCommand.hpp"
#include "action/EditorState.hpp"
#include "engine/editor/CommandResult.hpp"
#include "engine/editor/EditorSession.hpp"
#include "play/GameLauncher.hpp"
#include "play/PlaySession.hpp"
#include "settings/Settings.hpp"



namespace editor {

/**
 * @brief Owns the session and the editor state, and applies every Action.
 *
 * The UI reads state() and session() and never changes them: it emits actions, which the loop
 * hands to handle() after the frame is drawn.
 */
class Editor {
public:
    /**
     * @param scratch Folder for the scene file Play writes; the default is in the system's
     * temporary folder.
     */
    explicit Editor(engine::editor::EditorSession session,
                    std::filesystem::path scratch = default_scratch());

    /** @brief The folder Play writes to when none is given. */
    [[nodiscard]] static std::filesystem::path default_scratch();

    /** @brief Lets Play start games; without a launcher Play is refused. */
    void attach_launcher(std::unique_ptr<GameLauncher> launcher);

    /** @brief Stamps console lines, e.g. with the local time; without it lines have no time. */
    void attach_clock(std::function<std::string()> clock);

    /** @brief Lets OpenScript open files; without it OpenScript is refused. */
    void attach_file_opener(FileOpener opener);
    /**
     * @brief Starts the view with the settings' defaults and writes each settings problem to the
     * console and the log.
     */
    void apply_settings(const LoadedSettings& loaded);
    /** @brief Sets the folder of the runtime binaries an export copies. */
    void attach_runtime(std::filesystem::path folder);

    /**
     * @brief Follows the played game: its output goes to the console, and its end brings the
     * editor back to Mode::editing. Called once per frame by the loop.
     */
    void update();

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
    [[nodiscard]] engine::editor::CommandResult reload_settings();
    void refresh_lists();
    void refresh_inspector();
    void report(const engine::editor::CommandResult& result);

    engine::editor::EditorSession _session;
    EditorState _state;
    PlaySession _play;
    FileOpener _open_file;
    std::filesystem::path _runtime;
    std::function<std::string()> _clock;

    void log(ConsoleLine line);
};

} // namespace editor
