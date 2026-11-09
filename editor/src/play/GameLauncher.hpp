#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace editor {

/** @brief What to play: a game folder, started from a scene file instead of its start scene. */
struct PlayRequest {
    std::filesystem::path game;
    std::filesystem::path scene_file;
};

/** @brief A game being played, outside the editor. */
class GameRun {
public:
    GameRun() = default;
    GameRun(const GameRun&) = delete;
    GameRun& operator=(const GameRun&) = delete;
    GameRun(GameRun&&) = delete;
    GameRun& operator=(GameRun&&) = delete;
    /** @brief Stops the game if it still runs. */
    virtual ~GameRun() = default;

    /** @return The complete lines the game printed since the last call, oldest first. */
    [[nodiscard]] virtual std::vector<std::string> take_output() = 0;
    /** @return The exit code once the game ended, std::nullopt while it runs. */
    [[nodiscard]] virtual std::optional<int> exit_code() = 0;
    virtual void pause() = 0;
    virtual void resume() = 0;
    /** @brief Ends the game and waits for it. */
    virtual void stop() = 0;
};

/** @brief Starts games; the editor gets one from its front, tests give a fake. */
class GameLauncher {
public:
    GameLauncher() = default;
    GameLauncher(const GameLauncher&) = delete;
    GameLauncher& operator=(const GameLauncher&) = delete;
    GameLauncher(GameLauncher&&) = delete;
    GameLauncher& operator=(GameLauncher&&) = delete;
    virtual ~GameLauncher() = default;

    /** @brief The started game, or why it could not start. */
    struct Launch {
        std::unique_ptr<GameRun> run;
        std::string error;
    };

    [[nodiscard]] virtual Launch launch(const PlayRequest& request) = 0;
};

} // namespace editor
