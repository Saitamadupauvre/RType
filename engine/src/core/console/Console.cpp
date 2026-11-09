#include "engine/core/console/Console.hpp"

#include <algorithm>
#include <exception>
#include <format>
#include <iterator>
#include <utility>

namespace engine::core::console {

namespace {

constexpr std::string_view server_prefix = "/server";

bool continuation_byte(char byte) { return (static_cast<unsigned char>(byte) & 0xC0U) == 0x80U; }

bool control_character(char byte) {
    const auto code = static_cast<unsigned char>(byte);
    return code < 0x20U || code == 0x7FU;
}

std::string encode_utf8(char32_t character) {
    std::string encoded;
    const auto code = static_cast<std::uint32_t>(character);
    if (code < 0x80U) {
        encoded.push_back(static_cast<char>(code));
    } else if (code < 0x800U) {
        encoded.push_back(static_cast<char>(0xC0U | (code >> 6U)));
        encoded.push_back(static_cast<char>(0x80U | (code & 0x3FU)));
    } else if (code < 0x10000U) {
        encoded.push_back(static_cast<char>(0xE0U | (code >> 12U)));
        encoded.push_back(static_cast<char>(0x80U | ((code >> 6U) & 0x3FU)));
        encoded.push_back(static_cast<char>(0x80U | (code & 0x3FU)));
    } else if (code < 0x110000U) {
        encoded.push_back(static_cast<char>(0xF0U | (code >> 18U)));
        encoded.push_back(static_cast<char>(0x80U | ((code >> 12U) & 0x3FU)));
        encoded.push_back(static_cast<char>(0x80U | ((code >> 6U) & 0x3FU)));
        encoded.push_back(static_cast<char>(0x80U | (code & 0x3FU)));
    }
    return encoded;
}

std::string_view trim(std::string_view text) {
    const auto first = text.find_first_not_of(" \t");
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = text.find_last_not_of(" \t");
    return text.substr(first, last - first + 1);
}

std::pair<std::string_view, std::string_view> split_word(std::string_view text) {
    const std::string_view trimmed = trim(text);
    const auto space = trimmed.find_first_of(" \t");
    if (space == std::string_view::npos) {
        return {trimmed, {}};
    }
    return {trimmed.substr(0, space), trim(trimmed.substr(space))};
}

std::string describe_range(const CVarInfo& info) {
    if (!info.range) {
        return {};
    }
    return std::format(", {} to {}", info.range->min, info.range->max);
}

} // namespace

Console::Console(CVars& cvars, std::size_t max_lines, std::size_t max_history)
    : _cvars(cvars), _max_lines(std::max<std::size_t>(max_lines, 1)),
      _max_history(std::max<std::size_t>(max_history, 1)) {}

void Console::set_lua(Executor run) { _lua = std::move(run); }

void Console::set_server_lua(Executor run) { _server_lua = std::move(run); }

bool Console::is_open() const noexcept { return _open; }

void Console::set_open(bool open) noexcept { _open = open; }

void Console::type(std::string_view text) {
    std::string kept;
    std::ranges::copy_if(text, std::back_inserter(kept),
                         [](char byte) { return !control_character(byte); });
    _input.insert(_cursor, kept);
    _cursor += kept.size();
}

void Console::type(char32_t character) { type(encode_utf8(character)); }

void Console::erase_before() {
    if (_cursor == 0) {
        return;
    }
    const std::size_t end = _cursor;
    move_left();
    _input.erase(_cursor, end - _cursor);
}

void Console::erase_after() {
    if (_cursor == _input.size()) {
        return;
    }
    const std::size_t start = _cursor;
    move_right();
    _input.erase(start, _cursor - start);
    _cursor = start;
}

void Console::move_left() noexcept {
    while (_cursor > 0) {
        --_cursor;
        if (!continuation_byte(_input[_cursor])) {
            return;
        }
    }
}

void Console::move_right() noexcept {
    if (_cursor == _input.size()) {
        return;
    }
    ++_cursor;
    while (_cursor < _input.size() && continuation_byte(_input[_cursor])) {
        ++_cursor;
    }
}

void Console::move_home() noexcept { _cursor = 0; }

void Console::move_end() noexcept { _cursor = _input.size(); }

void Console::history_previous() {
    if (_history.empty()) {
        return;
    }
    if (!_recalled) {
        _draft = _input;
        recall(_history.size() - 1);
    } else if (*_recalled > 0) {
        recall(*_recalled - 1);
    }
}

void Console::history_next() {
    if (!_recalled) {
        return;
    }
    if (*_recalled + 1 < _history.size()) {
        recall(*_recalled + 1);
        return;
    }
    _recalled.reset();
    _input = std::exchange(_draft, {});
    _cursor = _input.size();
}

void Console::recall(std::size_t index) {
    _recalled = index;
    _input = _history[index];
    _cursor = _input.size();
}

void Console::scroll(int lines) noexcept {
    const auto newest = static_cast<long long>(_lines.size()) - 1;
    const auto wanted = static_cast<long long>(_scroll) + lines;
    _scroll = static_cast<std::size_t>(std::clamp<long long>(wanted, 0, std::max(newest, 0LL)));
}

std::size_t Console::scroll_offset() const noexcept { return _scroll; }

void Console::submit() {
    const std::string line = std::exchange(_input, {});
    _cursor = 0;
    _recalled.reset();
    _draft.clear();
    _scroll = 0;
    if (trim(line).empty()) {
        return;
    }
    print(LineKind::input, "> " + line);
    if (_history.empty() || _history.back() != line) {
        _history.push_back(line);
        if (_history.size() > _max_history) {
            _history.erase(_history.begin());
        }
    }
    execute(line);
}

void Console::execute(std::string_view line) {
    try {
        run_command(line);
    } catch (const std::exception& exception) {
        print(LineKind::error, exception.what());
    } catch (...) {
        print(LineKind::error, "unknown error");
    }
}

void Console::run_command(std::string_view line) {
    const auto [word, arguments] = split_word(line);
    if (word.empty()) {
        return;
    }
    if (word == "help") {
        show_help();
    } else if (word == "clear") {
        clear();
    } else if (word == "cvar") {
        run_cvar(arguments);
    } else if (word == server_prefix) {
        run_code(_server_lua, arguments,
                 "no local server: /server only works in solo or host mode");
    } else {
        run_code(_lua, trim(line), "no Lua runtime is attached");
    }
}

void Console::run_code(const Executor& run, std::string_view code, std::string_view missing) {
    if (!run) {
        print(LineKind::error, missing);
        return;
    }
    if (code.empty()) {
        print(LineKind::error, "nothing to run");
        return;
    }
    const ExecutionResult result = run(code);
    if (!result.output.empty()) {
        print(result.ok ? LineKind::output : LineKind::error, result.output);
    }
}

void Console::show_help() {
    print(LineKind::output, "help                    list the commands");
    print(LineKind::output, "clear                   empty the console");
    print(LineKind::output, "cvar [list] [filter]    list the variables");
    print(LineKind::output, "cvar <name>             show a variable");
    print(LineKind::output, "cvar <name> <value>     change a variable");
    print(LineKind::output, "cvar reset <name>       restore its default");
    print(LineKind::output, "/server <lua>           run Lua on the local server");
    print(LineKind::output, "<lua>                   run Lua in the client, e.g. 1 + 1");
}

void Console::run_cvar(std::string_view arguments) {
    const auto [first, rest] = split_word(arguments);
    if (first.empty() || first == "list") {
        list_cvars(first.empty() ? std::string_view{} : rest);
        return;
    }
    if (first == "reset") {
        report(_cvars.reset(rest), rest);
        return;
    }
    if (rest.empty()) {
        const auto info = _cvars.find(first);
        if (!info) {
            print(LineKind::error, std::format("unknown variable {}", first));
            return;
        }
        show_cvar(*info);
        return;
    }
    report(_cvars.set_from_string(first, rest), first);
}

void Console::list_cvars(std::string_view filter) {
    std::size_t shown = 0;
    for (const CVarInfo& info : _cvars.list()) {
        if (info.name.find(filter) == std::string::npos) {
            continue;
        }
        print(LineKind::output,
              std::format("{} = {}  {}", info.name, to_string(info.value), info.description));
        ++shown;
    }
    if (shown == 0) {
        print(LineKind::output, "no variable");
    }
}

void Console::show_cvar(const CVarInfo& info) {
    print(LineKind::output,
          std::format("{} = {} ({}, default {}{})", info.name, to_string(info.value),
                      kind_name(info.kind), to_string(info.default_value), describe_range(info)));
    if (!info.description.empty()) {
        print(LineKind::output, info.description);
    }
}

void Console::report(const CVarResult& result, std::string_view name) {
    if (!result) {
        print(LineKind::error, result.error);
        return;
    }
    if (const auto value = _cvars.get(name)) {
        print(LineKind::output, std::format("{} = {}", name, to_string(*value)));
    }
}

void Console::print(LineKind kind, std::string_view text) {
    std::size_t start = 0;
    while (start <= text.size()) {
        const auto end = text.find('\n', start);
        const auto length = end == std::string_view::npos ? text.size() - start : end - start;
        _lines.push_back({.kind = kind, .text = std::string(text.substr(start, length))});
        if (_lines.size() > _max_lines) {
            _lines.pop_front();
        }
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
    _scroll = std::min(_scroll, _lines.empty() ? 0 : _lines.size() - 1);
}

void Console::clear() noexcept {
    _lines.clear();
    _scroll = 0;
}

const std::string& Console::input() const noexcept { return _input; }

std::size_t Console::cursor() const noexcept { return _cursor; }

const std::deque<ConsoleLine>& Console::lines() const noexcept { return _lines; }

const std::vector<std::string>& Console::history() const noexcept { return _history; }

} // namespace engine::core::console
