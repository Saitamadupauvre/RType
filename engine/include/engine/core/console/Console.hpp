#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "engine/core/Export.hpp"
#include "engine/core/console/CVars.hpp"

namespace engine::core::console {

/** @brief What running a line of code produced. */
struct ExecutionResult {
    bool ok{true};
    std::string output;
};

/**
 * @brief Runs a line of code, e.g. Lua in a script runtime.
 *
 * @note Should not throw; the console turns an exception into an error line.
 */
using Executor = std::function<ExecutionResult(std::string_view code)>;

/** @brief Origin of a scrollback line, used to color it. */
enum class LineKind : std::uint8_t { input, output, error, log };

/** @brief One line of the console scrollback. */
struct ConsoleLine {
    LineKind kind;
    std::string text;
};

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

/**
 * @brief Developer console: an editable input line, a history of submitted lines, a bounded
 * scrollback and the built-in commands, without any display or keyboard.
 *
 * A submitted line runs, in order of precedence:
 * - `help`, `clear` and `cvar ...`, the built-in commands;
 * - `/server <code>`, through the server executor;
 * - anything else, through the Lua executor.
 *
 * Errors are printed to the scrollback, never thrown.
 *
 * @note Not thread safe: use it from one thread, e.g. the main thread of the client.
 */
class ENGINE_CORE_EXPORT Console {
public:
    /** @brief Default number of lines kept in the scrollback. */
    static constexpr std::size_t default_max_lines = 512;
    /** @brief Default number of submitted lines kept in the history. */
    static constexpr std::size_t default_max_history = 64;

    /**
     * @param cvars Registry read and written by the `cvar` command; must outlive the console.
     * @param max_lines Size of the scrollback; the oldest lines are dropped beyond it.
     * @param max_history Size of the history; the oldest entries are dropped beyond it.
     */
    explicit Console(CVars& cvars, std::size_t max_lines = default_max_lines,
                     std::size_t max_history = default_max_history);

    /** @brief Sets the executor of plain lines; an empty function disables it. */
    void set_lua(Executor run);

    /** @brief Sets the executor of `/server` lines; an empty function disables it. */
    void set_server_lua(Executor run);

    /** @return true when the console is shown and takes the keyboard. */
    [[nodiscard]] bool is_open() const noexcept;

    /** @brief Shows or hides the console. */
    void set_open(bool open) noexcept;

    /** @brief Inserts text at the cursor; control characters are dropped. */
    void type(std::string_view text);

    /** @brief Inserts one Unicode character at the cursor, encoded in UTF-8. */
    void type(char32_t character);

    /** @brief Erases the character before the cursor, like Backspace. */
    void erase_before();

    /** @brief Erases the character after the cursor, like Delete. */
    void erase_after();

    /** @brief Moves the cursor one character to the left. */
    void move_left() noexcept;

    /** @brief Moves the cursor one character to the right. */
    void move_right() noexcept;

    /** @brief Moves the cursor to the start of the input. */
    void move_home() noexcept;

    /** @brief Moves the cursor to the end of the input. */
    void move_end() noexcept;

    /** @brief Replaces the input with the previous history entry, keeping the draft. */
    void history_previous();

    /** @brief Replaces the input with the next history entry, or back with the draft. */
    void history_next();

    /** @brief Scrolls the scrollback up by @p lines, towards older lines (negative: down). */
    void scroll(int lines) noexcept;

    /** @return How many lines the view is scrolled up from the newest line. */
    [[nodiscard]] std::size_t scroll_offset() const noexcept;

    /** @brief Echoes the input, adds it to the history, runs it and clears it. */
    void submit();

    /** @brief Runs one line as if submitted, without echo nor history. */
    void execute(std::string_view line);

    /** @brief Appends text to the scrollback, one line per '\n'. */
    void print(LineKind kind, std::string_view text);

    /** @brief Empties the scrollback. */
    void clear() noexcept;

    /** @return The text being edited. */
    [[nodiscard]] const std::string& input() const noexcept;

    /** @return The cursor position, as a byte offset in input(). */
    [[nodiscard]] std::size_t cursor() const noexcept;

    /** @return The scrollback, oldest line first. */
    [[nodiscard]] const std::deque<ConsoleLine>& lines() const noexcept;

    /** @return The submitted lines, oldest first. */
    [[nodiscard]] const std::vector<std::string>& history() const noexcept;

private:
    void run_command(std::string_view line);
    void run_code(const Executor& run, std::string_view code, std::string_view missing);
    void show_help();
    void run_cvar(std::string_view arguments);
    void list_cvars(std::string_view filter);
    void show_cvar(const CVarInfo& info);
    void report(const CVarResult& result, std::string_view name);
    void recall(std::size_t index);

    CVars& _cvars;
    Executor _lua;
    Executor _server_lua;
    std::size_t _max_lines;
    std::size_t _max_history;
    std::deque<ConsoleLine> _lines;
    std::vector<std::string> _history;
    std::optional<std::size_t> _recalled;
    std::string _draft;
    std::string _input;
    std::size_t _cursor{0};
    std::size_t _scroll{0};
    bool _open{false};
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

} // namespace engine::core::console
