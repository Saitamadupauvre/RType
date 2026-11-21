#pragma once

#include <filesystem>
#include <string>
#include <vector>

#if !defined(_WIN32)
#include <sys/types.h>
#endif

#include "open/FileOpener.hpp"

namespace editor {

#if defined(_WIN32)
using NativeProcessHandle = void*;
#else
using NativeProcessHandle = pid_t;
#endif

/**
 * @brief Opens files by running `program <file>` in its own session, its input and output on
 * /dev/null (or detached from the console on Windows), so a terminal program never takes the
 * editor's terminal. Openers still running when the editor quits are left running. An empty
 * program means the operating system's own default opener for the file.
 */
class SystemOpener final : public FileOpener {
public:
    /** @param program Found in PATH unless it has a slash; xdg-open, open on macOS, or empty on
     * Windows to use the file's own default association. */
    explicit SystemOpener(std::filesystem::path program = default_program());

    [[nodiscard]] static std::filesystem::path default_program();

    [[nodiscard]] bool open(const std::filesystem::path& file) override;
    [[nodiscard]] std::vector<std::string> take_failures() override;

private:
    struct Opening {
        NativeProcessHandle handle;
        std::filesystem::path file;
    };

    std::filesystem::path _program;
    std::vector<Opening> _running;
};

} // namespace editor
