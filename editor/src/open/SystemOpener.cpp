#include "open/SystemOpener.hpp"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <shellapi.h>
#else
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#ifdef __APPLE__
#include <crt_externs.h>
#endif

#include <array>
#include <format>
#include <fstream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace editor {

namespace {

#if defined(_WIN32)

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

HANDLE open_null_device(bool for_write) {
    SECURITY_ATTRIBUTES security{};
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;
    return CreateFileW(L"NUL", for_write ? GENERIC_WRITE : GENERIC_READ,
                       FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING, 0, nullptr);
}

std::wstring build_command_line(const std::filesystem::path& program,
                                const std::filesystem::path& file) {
    std::vector<std::string> argv;
    if (auto interpreter = shebang_interpreter(program)) {
        argv = std::move(*interpreter);
    }
    argv.push_back(program.string());
    argv.push_back(file.string());

    std::wstring command_line;
    for (const std::string& argument : argv) {
        if (!command_line.empty()) {
            command_line.push_back(L' ');
        }
        command_line += quote_argument(argument);
    }
    return command_line;
}

#else

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

#endif

} // namespace

SystemOpener::SystemOpener(std::filesystem::path program) : _program(std::move(program)) {}

std::filesystem::path SystemOpener::default_program() {
#if defined(_WIN32)
    return {};
#elif defined(__APPLE__)
    return "open";
#else
    return "xdg-open";
#endif
}

#if defined(_WIN32)

bool SystemOpener::open(const std::filesystem::path& file) {
    if (_program.empty()) {
        SHELLEXECUTEINFOW info{};
        info.cbSize = sizeof(info);
        info.fMask = SEE_MASK_NOCLOSEPROCESS;
        info.lpVerb = L"open";
        const std::wstring file_text = file.wstring();
        info.lpFile = file_text.c_str();
        info.nShow = SW_SHOWNORMAL;
        if (ShellExecuteExW(&info) == FALSE) {
            return false;
        }
        if (info.hProcess != nullptr) {
            _running.push_back({.handle = info.hProcess, .file = file});
        }
        return true;
    }

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = open_null_device(false);
    startup.hStdOutput = open_null_device(true);
    startup.hStdError = startup.hStdOutput;
    PROCESS_INFORMATION process{};
    std::wstring command_line = build_command_line(_program, file);
    const BOOL started = CreateProcessW(nullptr, command_line.data(), nullptr, nullptr, TRUE,
                                        CREATE_NEW_PROCESS_GROUP | CREATE_NO_WINDOW, nullptr,
                                        nullptr, &startup, &process);
    CloseHandle(startup.hStdInput);
    CloseHandle(startup.hStdOutput);
    if (started == FALSE) {
        return false;
    }
    CloseHandle(process.hThread);
    _running.push_back({.handle = process.hProcess, .file = file});
    return true;
}

std::vector<std::string> SystemOpener::take_failures() {
    std::vector<std::string> failures;
    std::erase_if(_running, [&](const Opening& opening) {
        if (WaitForSingleObject(opening.handle, 0) != WAIT_OBJECT_0) {
            return false;
        }
        DWORD exit_code = 0;
        const bool exited_ok = GetExitCodeProcess(opening.handle, &exit_code) != 0;
        CloseHandle(opening.handle);
        if (exited_ok && exit_code != 0) {
            const std::string program_name =
                _program.empty() ? std::string("the default opener") : _program.filename().string();
            failures.push_back(std::format(
                "{} could not open {} (exit code {}); check which program opens this kind of file.",
                program_name, opening.file.string(), exit_code));
        }
        return true;
    });
    return failures;
}

#else

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
    _running.push_back({.handle = pid, .file = file});
    return true;
}

std::vector<std::string> SystemOpener::take_failures() {
    std::vector<std::string> failures;
    std::erase_if(_running, [&](const Opening& opening) {
        int status = 0;
        if (::waitpid(opening.handle, &status, WNOHANG) != opening.handle) {
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

#endif

} // namespace editor
