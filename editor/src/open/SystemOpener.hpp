#pragma once

#include <filesystem>
#include <string>
#include <sys/types.h>
#include <vector>

#include "open/FileOpener.hpp"

namespace editor {

/**
 * @brief Opens files by running `program <file>` in its own session, its input and output on
 * /dev/null, so a terminal program never takes the editor's terminal. Openers still running when
 * the editor quits are left running.
 * @note POSIX only.
 */
class SystemOpener final : public FileOpener {
public:
    /** @param program Found in PATH unless it has a slash; xdg-open, or open on macOS. */
    explicit SystemOpener(std::filesystem::path program = default_program());

    [[nodiscard]] static std::filesystem::path default_program();

    [[nodiscard]] bool open(const std::filesystem::path& file) override;
    [[nodiscard]] std::vector<std::string> take_failures() override;

private:
    struct Opening {
        pid_t pid;
        std::filesystem::path file;
    };

    std::filesystem::path _program;
    std::vector<Opening> _running;
};

} // namespace editor
