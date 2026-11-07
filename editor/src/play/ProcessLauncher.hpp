#pragma once

#include <filesystem>

#include "play/GameLauncher.hpp"

namespace editor {

/**
 * @brief Plays a game by running `program --solo --game <game> --scene-file <scene>` as a child
 * process, its output and errors merged into lines. Pause and resume stop and continue the
 * process; stop asks it to quit, then kills it if it does not.
 * @note POSIX only.
 */
class ProcessLauncher final : public GameLauncher {
public:
    explicit ProcessLauncher(std::filesystem::path program);

    [[nodiscard]] Launch launch(const PlayRequest& request) override;

private:
    std::filesystem::path _program;
};

} // namespace editor
