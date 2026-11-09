#include "play/ProcessLauncher.hpp"

#include <fcntl.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef __APPLE__
#include <crt_externs.h>
#endif

#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <format>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace editor {

namespace {

char** environment() {
#ifdef __APPLE__
    return *_NSGetEnviron();
#else
    return environ;
#endif
}

void make_non_blocking(int descriptor) {
    const auto flags = static_cast<unsigned int>(::fcntl(descriptor, F_GETFL));
    ::fcntl(descriptor, F_SETFL, static_cast<int>(flags | static_cast<unsigned int>(O_NONBLOCK)));
}

constexpr int quit_attempts = 100;
constexpr std::chrono::milliseconds quit_poll{10};
constexpr std::size_t read_chunk = 4096;

class ProcessRun final : public GameRun {
public:
    ProcessRun(pid_t pid, int output) : _pid(pid), _output(output) {}

    ProcessRun(const ProcessRun&) = delete;
    ProcessRun& operator=(const ProcessRun&) = delete;
    ProcessRun(ProcessRun&&) = delete;
    ProcessRun& operator=(ProcessRun&&) = delete;

    ~ProcessRun() override {
        stop();
        ::close(_output);
    }

    std::vector<std::string> take_output() override {
        std::array<char, read_chunk> buffer{};
        ssize_t count = 0;
        while ((count = ::read(_output, buffer.data(), buffer.size())) > 0) {
            _pending.append(buffer.data(), static_cast<std::size_t>(count));
        }
        std::vector<std::string> lines;
        std::size_t end = 0;
        while ((end = _pending.find('\n')) != std::string::npos) {
            lines.push_back(_pending.substr(0, end));
            _pending.erase(0, end + 1);
        }
        if (_exit && !_pending.empty()) {
            lines.push_back(std::exchange(_pending, {}));
        }
        return lines;
    }

    std::optional<int> exit_code() override {
        if (!_exit) {
            int status = 0;
            if (::waitpid(_pid, &status, WNOHANG) == _pid) {
                _exit = exit_of(status);
            }
        }
        return _exit;
    }

    void pause() override {
        if (!_exit) {
            ::kill(_pid, SIGSTOP);
        }
    }

    void resume() override {
        if (!_exit) {
            ::kill(_pid, SIGCONT);
        }
    }

    void stop() override {
        if (exit_code()) {
            return;
        }
        ::kill(_pid, SIGCONT);
        ::kill(_pid, SIGTERM);
        for (int attempt = 0; attempt < quit_attempts && !exit_code(); ++attempt) {
            std::this_thread::sleep_for(quit_poll);
        }
        if (!_exit) {
            ::kill(_pid, SIGKILL);
            int status = 0;
            ::waitpid(_pid, &status, 0);
            _exit = exit_of(status);
        }
    }

private:
    static int exit_of(int status) {
        return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
    }

    pid_t _pid;
    int _output;
    std::string _pending;
    std::optional<int> _exit;
};

struct Pipe {
    std::array<int, 2> ends{-1, -1};

    Pipe() = default;
    Pipe(const Pipe&) = delete;
    Pipe& operator=(const Pipe&) = delete;
    Pipe(Pipe&&) = delete;
    Pipe& operator=(Pipe&&) = delete;
    ~Pipe() {
        for (const int end : ends) {
            if (end >= 0) {
                ::close(end);
            }
        }
    }

    int release_read() { return std::exchange(ends[0], -1); }
};

} // namespace

ProcessLauncher::ProcessLauncher(std::filesystem::path program) : _program(std::move(program)) {}

GameLauncher::Launch ProcessLauncher::launch(const PlayRequest& request) {
    if (!std::filesystem::is_regular_file(_program)) {
        return {.run = nullptr, .error = std::format("no game runtime at {}", _program.string())};
    }
    Pipe output;
    if (::pipe(output.ends.data()) != 0) {
        return {.run = nullptr,
                .error = std::format("cannot start the game: {}", std::strerror(errno))};
    }
    make_non_blocking(output.ends[0]);
    ::fcntl(output.ends[0], F_SETFD, FD_CLOEXEC);
    ::fcntl(output.ends[1], F_SETFD, FD_CLOEXEC);

    std::vector<std::string> arguments{_program.string(), "--solo",
                                       "--game",          request.game.string(),
                                       "--scene-file",    request.scene_file.string()};
    std::vector<char*> argv;
    argv.reserve(arguments.size() + 1);
    for (std::string& argument : arguments) {
        argv.push_back(argument.data());
    }
    argv.push_back(nullptr);

    posix_spawn_file_actions_t actions{};
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, output.ends[1], STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions, output.ends[1], STDERR_FILENO);
    pid_t pid = 0;
    const int failed = posix_spawn(&pid, argv[0], &actions, nullptr, argv.data(), environment());
    posix_spawn_file_actions_destroy(&actions);
    if (failed != 0) {
        return {.run = nullptr,
                .error = std::format("cannot start the game: {}", std::strerror(failed))};
    }
    return {.run = std::make_unique<ProcessRun>(pid, output.release_read()), .error = {}};
}

} // namespace editor
