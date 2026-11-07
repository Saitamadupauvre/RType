#include "cli/Cli.hpp"

#include <system_error>

#include "clap.hpp"

namespace editor {

namespace {

constexpr int error_exit_code = 84;

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

    app.example(program + " game", "Edit the game in ./game and open its start scene.");

    const bool ok = app.parse(args);

    if (help || !ok) {
        return {.config = std::nullopt,
                .exit_code = help ? 0 : error_exit_code,
                .message = help ? app.help() : app.error()};
    }
    return {.config = EditorConfig{.game = game.get()}, .exit_code = 0, .message = ""};
}

} // namespace editor
