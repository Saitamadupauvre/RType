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

std::string game_problem(const std::filesystem::path& folder) {
    std::error_code error;
    if (folder.empty()) {
        return "missing game folder: give one, or --new to create a game";
    }
    if (!std::filesystem::is_directory(folder, error)) {
        return folder.string() + " is not a folder";
    }
    if (!std::filesystem::is_regular_file(folder / "game.json", error)) {
        return "no game.json in " + folder.string() + "; use --new to create a game there";
    }
    return "";
}

std::string new_game_problem(const std::filesystem::path& folder) {
    std::error_code error;
    if (!std::filesystem::exists(folder, error)) {
        return "";
    }
    if (!std::filesystem::is_directory(folder, error)) {
        return folder.string() + " is a file";
    }
    if (!std::filesystem::is_empty(folder, error)) {
        return folder.string() + " is not empty; --new needs an empty or missing folder";
    }
    return "";
}

} // namespace

CliResult parse_cli(const std::vector<std::string>& args) {
    const std::string program = args.empty() ? "editor" : args[0];
    clap::App app(program, "The editor for games made with the engine");

    auto& help = app.flag("-h,--help", "Show this help message.");
    auto& game = app.positional<std::filesystem::path>(
                        "game", "Game folder to edit; must contain game.json. With --new, the "
                                "folder to create, the current one by default.")
                     .default_value(std::filesystem::path{});

    auto& create =
        app.flag("--new", "Create a new game in the folder (empty or missing), then open it.");

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
    app.example(program + " --new my-game", "Create a game in ./my-game and open it.");
    app.example(program + " --new", "Create a game in the current folder, which must be empty.");
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
    const std::filesystem::path folder =
        create && game.get().empty() ? std::filesystem::path(".") : game.get();
    if (const std::string problem = create ? new_game_problem(folder) : game_problem(folder);
        !problem.empty()) {
        return {.config = std::nullopt,
                .exit_code = error_exit_code,
                .message = "error: " + problem + "\n"};
    }
    return {.config = EditorConfig{.game = folder,
                                   .create = create,
                                   .runtime = runtime.get(),
                                   .log_level = level_named(log_level.get())},
            .exit_code = 0,
            .message = ""};
}

} // namespace editor
