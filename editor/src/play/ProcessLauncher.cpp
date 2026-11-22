#include "play/ProcessLauncher.hpp"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <tlhelp32.h>
#else
#include <fcntl.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#ifdef __APPLE__
#include <crt_externs.h>
#endif

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <format>
#include <fstream>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace editor {

namespace {

constexpr int quit_attempts = 100;
constexpr std::chrono::milliseconds quit_poll{10};
constexpr std::size_t read_chunk = 4096;

#if defined(_WIN32)

constexpr int terminated_exit_code = 128 + 15;

std::optional<std::vector<std::string>> shebang_interpreter(const std::filesystem::path& program) {
    std::ifstream stream(program);
    std::string line;
    if (!std::getline(stream, line) || line.size() < 2 || line[0] != '#' || line[1] != '!') {
        return std::nullopt;
    }
    std::vector<std::string> tokens;
    std::size_t start = 2;
    while (start < line.size()) {
        const std::size_t end = line.find_first_of(" \t\r", start);
        std::string token = line.substr(start, end == std::string::npos ? end : end - start);
        if (!token.empty()) {
            tokens.push_back(std::filesystem::path(token).filename().string());
        }
        if (end == std::string::npos) {
            break;
        }
        start = line.find_first_not_of(" \t\r", end);
    }
    if (tokens.empty()) {
        return std::nullopt;
    }
    return tokens;
}

std::wstring quote_argument(const std::string& argument) {
    std::wstring wide(argument.begin(), argument.end());
    if (!wide.empty() && wide.find_first_of(L" \t\"") == std::wstring::npos) {
        return wide;
    }
    std::wstring quoted = L"\"";
    for (std::size_t i = 0; i < wide.size(); ++i) {
        std::size_t backslashes = 0;
        while (i < wide.size() && wide[i] == L'\\') {
            ++backslashes;
            ++i;
        }
        if (i == wide.size()) {
            quoted.append(backslashes * 2, L'\\');
            break;
        }
        if (wide[i] == L'"') {
            quoted.append(backslashes * 2 + 1, L'\\');
            quoted.push_back(L'"');
        } else {
            quoted.append(backslashes, L'\\');
            quoted.push_back(wide[i]);
        }
    }
    quoted.push_back(L'"');
    return quoted;
}

std::wstring build_command_line(const std::vector<std::string>& arguments) {
    std::vector<std::string> argv;
    if (auto interpreter = shebang_interpreter(arguments.front())) {
        argv = std::move(*interpreter);
    }
    argv.insert(argv.end(), arguments.begin(), arguments.end());

    std::wstring command_line;
    for (const std::string& argument : argv) {
        if (!command_line.empty()) {
            command_line.push_back(L' ');
        }
        command_line += quote_argument(argument);
    }
    return command_line;
}

void set_threads_suspended(DWORD process_id, bool suspended) {
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return;
    }
    THREADENTRY32 entry{};
    entry.dwSize = sizeof(entry);
    if (Thread32First(snapshot, &entry)) {
        do {
            if (entry.th32OwnerProcessID != process_id) {
                continue;
            }
            if (const HANDLE thread = OpenThread(THREAD_SUSPEND_RESUME, FALSE, entry.th32ThreadID);
                thread != nullptr) {
                if (suspended) {
                    SuspendThread(thread);
                } else {
                    ResumeThread(thread);
                }
                CloseHandle(thread);
            }
        } while (Thread32Next(snapshot, &entry));
    }
    CloseHandle(snapshot);
}

class ProcessRun final : public GameRun {
public:
    ProcessRun(HANDLE process, DWORD process_id, HANDLE output)
        : _process(process), _process_id(process_id), _output(output) {}

    ProcessRun(const ProcessRun&) = delete;
    ProcessRun& operator=(const ProcessRun&) = delete;
    ProcessRun(ProcessRun&&) = delete;
    ProcessRun& operator=(ProcessRun&&) = delete;

    ~ProcessRun() override {
        stop();
        CloseHandle(_output);
        CloseHandle(_process);
    }

    std::vector<std::string> take_output() override {
        for (;;) {
            DWORD available = 0;
            if (PeekNamedPipe(_output, nullptr, 0, nullptr, &available, nullptr) == 0 ||
                available == 0) {
                break;
            }
            std::array<char, read_chunk> buffer{};
            DWORD read_bytes = 0;
            const DWORD to_read = std::min<DWORD>(available, static_cast<DWORD>(buffer.size()));
            if (ReadFile(_output, buffer.data(), to_read, &read_bytes, nullptr) == 0 ||
                read_bytes == 0) {
                break;
            }
            _pending.append(buffer.data(), read_bytes);
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
            DWORD code = 0;
            if (GetExitCodeProcess(_process, &code) != 0 && code != STILL_ACTIVE) {
                _exit = static_cast<int>(code);
            }
        }
        return _exit;
    }

    void pause() override {
        if (!_exit) {
            set_threads_suspended(_process_id, true);
        }
    }

    void resume() override {
        if (!_exit) {
            set_threads_suspended(_process_id, false);
        }
    }

    void stop() override {
        if (exit_code()) {
            return;
        }
        set_threads_suspended(_process_id, false);
        TerminateProcess(_process, static_cast<UINT>(terminated_exit_code));
        for (int attempt = 0; attempt < quit_attempts && !exit_code(); ++attempt) {
            std::this_thread::sleep_for(quit_poll);
        }
        if (!_exit) {
            WaitForSingleObject(_process, INFINITE);
            exit_code();
        }
    }

private:
    HANDLE _process;
    DWORD _process_id;
    HANDLE _output;
    std::string _pending;
    std::optional<int> _exit;
};

#else

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

#endif

} // namespace

ProcessLauncher::ProcessLauncher(std::filesystem::path program) : _program(std::move(program)) {}

#if defined(_WIN32)

GameLauncher::Launch ProcessLauncher::launch(const PlayRequest& request) {
    if (!std::filesystem::is_regular_file(_program)) {
        return {.run = nullptr, .error = std::format("no game runtime at {}", _program.string())};
    }

    SECURITY_ATTRIBUTES security{};
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;
    HANDLE read_end = nullptr;
    HANDLE write_end = nullptr;
    if (CreatePipe(&read_end, &write_end, &security, 0) == 0) {
        return {.run = nullptr, .error = "cannot start the game: could not create a pipe"};
    }
    SetHandleInformation(read_end, HANDLE_FLAG_INHERIT, 0);

    std::vector<std::string> arguments{_program.string(), "--solo",
                                       "--game",          request.game.string(),
                                       "--scene-file",    request.scene_file.string()};
    std::wstring command_line = build_command_line(arguments);

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = write_end;
    startup.hStdError = write_end;
    PROCESS_INFORMATION process{};
    const BOOL started =
        CreateProcessW(nullptr, command_line.data(), nullptr, nullptr, TRUE,
                       CREATE_NEW_PROCESS_GROUP, nullptr, nullptr, &startup, &process);
    CloseHandle(write_end);
    if (started == FALSE) {
        CloseHandle(read_end);
        return {.run = nullptr,
                .error = std::format("cannot start the game: no runtime at {}", _program.string())};
    }
    CloseHandle(process.hThread);
    return {.run = std::make_unique<ProcessRun>(process.hProcess, process.dwProcessId, read_end),
            .error = {}};
}

#else

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

#endif

} // namespace editor
