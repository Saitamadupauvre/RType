#include "open/SystemOpener.hpp"

#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef __APPLE__
#include <crt_externs.h>
#endif

#include <array>
#include <format>
#include <string>
#include <utility>

namespace editor {

namespace {

char** environment() {
#ifdef __APPLE__
    return *_NSGetEnviron();
#else
    return environ;
#endif
}

class SpawnSetup {
public:
    SpawnSetup() {
        posix_spawn_file_actions_init(&_actions);
        posix_spawnattr_init(&_attributes);
        posix_spawn_file_actions_addopen(&_actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
        posix_spawn_file_actions_addopen(&_actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
        posix_spawn_file_actions_addopen(&_actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
        posix_spawnattr_setflags(&_attributes, POSIX_SPAWN_SETSID);
    }

    SpawnSetup(const SpawnSetup&) = delete;
    SpawnSetup& operator=(const SpawnSetup&) = delete;
    SpawnSetup(SpawnSetup&&) = delete;
    SpawnSetup& operator=(SpawnSetup&&) = delete;

    ~SpawnSetup() {
        posix_spawnattr_destroy(&_attributes);
        posix_spawn_file_actions_destroy(&_actions);
    }

    [[nodiscard]] const posix_spawn_file_actions_t* actions() const { return &_actions; }
    [[nodiscard]] const posix_spawnattr_t* attributes() const { return &_attributes; }

private:
    posix_spawn_file_actions_t _actions{};
    posix_spawnattr_t _attributes{};
};

} // namespace

SystemOpener::SystemOpener(std::filesystem::path program) : _program(std::move(program)) {}

std::filesystem::path SystemOpener::default_program() {
#ifdef __APPLE__
    return "open";
#else
    return "xdg-open";
#endif
}

bool SystemOpener::open(const std::filesystem::path& file) {
    std::string program = _program.string();
    std::string argument = file.string();
    std::array<char*, 3> argv{program.data(), argument.data(), nullptr};
    const SpawnSetup setup;
    pid_t pid = 0;
    if (posix_spawnp(&pid, program.c_str(), setup.actions(), setup.attributes(), argv.data(),
                     environment()) != 0) {
        return false;
    }
    _running.push_back({.pid = pid, .file = file});
    return true;
}

std::vector<std::string> SystemOpener::take_failures() {
    std::vector<std::string> failures;
    std::erase_if(_running, [&](const Opening& opening) {
        int status = 0;
        if (::waitpid(opening.pid, &status, WNOHANG) != opening.pid) {
            return false;
        }
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            failures.push_back(std::format(
                "{} could not open {} (exit code {}); check which program opens this kind of file.",
                _program.filename().string(), opening.file.string(), WEXITSTATUS(status)));
        }
        return true;
    });
    return failures;
}

} // namespace editor
