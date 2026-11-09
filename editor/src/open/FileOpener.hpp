#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace editor {

/** @brief Opens files with the system's program for them; the editor never waits for it. */
class FileOpener {
public:
    FileOpener() = default;
    FileOpener(const FileOpener&) = delete;
    FileOpener& operator=(const FileOpener&) = delete;
    FileOpener(FileOpener&&) = delete;
    FileOpener& operator=(FileOpener&&) = delete;
    virtual ~FileOpener() = default;

    /**
     * @brief Starts opening `file` and returns at once, even while the program it starts runs.
     * @return false when nothing could be started.
     */
    [[nodiscard]] virtual bool open(const std::filesystem::path& file) = 0;

    /**
     * @return One message per open that failed after it started, since the last call, oldest
     * first. Never blocks.
     */
    [[nodiscard]] virtual std::vector<std::string> take_failures() = 0;
};

} // namespace editor
