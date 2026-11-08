#include "cli/Cli.hpp"

#include <array>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "clap.hpp"

namespace editor {

namespace {

constexpr int error_exit_code = 84;

using engine::core::log::Level;

constexpr std::array levels{
    std::pair{std::string_view("trace"), Level::Trace},
    std::pair{std::string_view("debug"), Level::Debug},
    std::pair{std::string_view("info"), Level::Info},
    std::pair{std::string_view("warn"), Level::Warn},
    std::pair{std::string_view("error"), Level::Error},
    std::pair{std::string_view("off"), Level::Off},
};

std::vector<std::string> level_names() {
    std::vector<std::string> names;
    names.reserve(levels.size());
    for (const auto& [name, level] : levels) {
        names.emplace_back(name);
    }
    return names;
}

Level level_named(std::string_view name) {
    for (const auto& [candidate, level] : levels) {
        if (candidate == name) {
            return level;
        }
    }
    return Level::Info;
}

class GameFolderFn {
public:
    std::string operator()(const std::filesystem::path& folder) const {
        std::error_code error;
        if (!std::filesystem::is_regular_file(folder / "game.json", error)) {
            return "no game.json in " + folder.string();
        }
        return "";
    }

    [[nodiscard]] const char* hint() const { return "must contain game.json"; }
};

} // namespace

CliResult parse_cli(const std::vector<std::string>& args) {
    const std::string program = args.empty() ? "editor" : args[0];
    clap::App app(program, "The editor for games made with the engine");

    auto& help = app.flag("-h,--help", "Show this help message.");
    auto& game = app.positional<std::filesystem::path>("game", "Game folder to edit.")
                     .validator(clap::DirExists)
                     .validator(GameFolderFn{});

    auto& runtime =
        app.option<std::filesystem::path>("--runtime", "Program that plays the game; defaults to "
                                                       "engine-runtime next to the editor.")
            .default_value(std::filesystem::path{});

    auto& log_level =
        app.option<std::string>("--log-level", "Lowest level of the developer log on stderr: trace "
                                               "shows every key and shortcut, debug every action.")
            .validator(clap::Choices(level_names()))
            .default_value("info");

    app.example(program + " examples/r-type", "Edit the R-Type example and open its start scene.");
    app.example(program + " examples/r-type --runtime ./engine-runtime",
                "Play with a given runtime.");
    app.example(program + " examples/r-type --log-level trace",
                "Log every key, shortcut and action.");

    const bool ok = app.parse(args);

    if (help || !ok) {
        return {.config = std::nullopt,
                .exit_code = help ? 0 : error_exit_code,
                .message = help ? app.help() : app.error()};
    }
    return {.config = EditorConfig{.game = game.get(),
                                   .runtime = runtime.get(),
                                   .log_level = level_named(log_level.get())},
            .exit_code = 0,
            .message = ""};
}

} // namespace editor
