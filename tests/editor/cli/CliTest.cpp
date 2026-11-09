#include <gtest/gtest.h>

#include <filesystem>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include "../GameFixture.hpp"
#include "cli/Cli.hpp"

using editor::EditorConfig;
using editor::parse_cli;

namespace {

constexpr int error_exit_code = 84;

class EditorCli : public GameFixture {
protected:
    static std::vector<std::string> args(std::initializer_list<std::string> rest) {
        std::vector<std::string> all{"editor"};
        all.insert(all.end(), rest);
        return all;
    }

    static void expect_rejected(const std::vector<std::string>& arguments) {
        const auto result = parse_cli(arguments);

        EXPECT_FALSE(result.config.has_value());
        EXPECT_EQ(result.exit_code, error_exit_code);
        EXPECT_FALSE(result.message.empty());
    }
};

} // namespace

TEST_F(EditorCli, AcceptsAGameFolder) {
    const auto result = parse_cli(args({_game.path().string()}));

    EXPECT_EQ(result.config, (EditorConfig{.game = _game.path()}));
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_TRUE(result.message.empty());
}

TEST_F(EditorCli, PrintsHelpWithShortAndLongFlag) {
    for (const std::string flag : {"-h", "--help"}) {
        const auto result = parse_cli(args({flag}));

        EXPECT_FALSE(result.config.has_value()) << flag;
        EXPECT_EQ(result.exit_code, 0) << flag;
        EXPECT_NE(result.message.find("game.json"), std::string::npos) << flag;
    }
}

TEST_F(EditorCli, RejectsAMissingGameFolder) { expect_rejected(args({})); }

TEST_F(EditorCli, RejectsAFolderThatDoesNotExist) {
    expect_rejected(args({(_game.path() / "nowhere").string()}));
}

TEST_F(EditorCli, RejectsAFile) { expect_rejected(args({(_game.path() / "game.json").string()})); }

TEST_F(EditorCli, RejectsAFolderWithoutGameJson) {
    expect_rejected(args({(_game.path() / "scenes").string()}));
}

TEST_F(EditorCli, RejectsExtraArguments) {
    expect_rejected(args({_game.path().string(), "other"}));
    expect_rejected(args({_game.path().string(), "--unknown"}));
}

TEST_F(EditorCli, RuntimeDefaultsToEmpty) {
    const auto result = parse_cli(args({_game.path().string()}));

    ASSERT_TRUE(result.config.has_value());
    EXPECT_TRUE(result.config->runtime.empty());
}

TEST_F(EditorCli, AcceptsARuntime) {
    const auto result = parse_cli(args({_game.path().string(), "--runtime", "/opt/game/runtime"}));

    ASSERT_TRUE(result.config.has_value());
    EXPECT_EQ(result.config->runtime, std::filesystem::path("/opt/game/runtime"));
}

TEST_F(EditorCli, LogLevelDefaultsToInfo) {
    const auto result = parse_cli(args({_game.path().string()}));

    ASSERT_TRUE(result.config.has_value());
    EXPECT_EQ(result.config->log_level, engine::core::log::Level::Info);
}

TEST_F(EditorCli, AcceptsEveryLogLevel) {
    using engine::core::log::Level;
    for (const auto& [name, level] :
         {std::pair{"trace", Level::Trace}, std::pair{"debug", Level::Debug},
          std::pair{"info", Level::Info}, std::pair{"warn", Level::Warn},
          std::pair{"error", Level::Error}, std::pair{"off", Level::Off}}) {
        const auto result = parse_cli(args({_game.path().string(), "--log-level", name}));

        ASSERT_TRUE(result.config.has_value()) << name;
        EXPECT_EQ(result.config->log_level, level) << name;
    }
}

TEST_F(EditorCli, RejectsAnUnknownLogLevel) {
    expect_rejected(args({_game.path().string(), "--log-level", "loud"}));
    expect_rejected(args({_game.path().string(), "--log-level"}));
}

TEST_F(EditorCli, NewCreatesInTheCurrentFolderByDefault) {
    const auto empty = _game.path() / "empty";
    std::filesystem::create_directories(empty);
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(empty);
    const auto result = parse_cli(args({"--new"}));
    std::filesystem::current_path(previous);

    ASSERT_TRUE(result.config.has_value()) << result.message;
    EXPECT_TRUE(result.config->create);
    EXPECT_EQ(result.config->game, std::filesystem::path("."));
}

TEST_F(EditorCli, NewAcceptsAMissingFolder) {
    const auto folder = _game.path() / "new-game";

    const auto result = parse_cli(args({"--new", folder.string()}));

    ASSERT_TRUE(result.config.has_value()) << result.message;
    EXPECT_TRUE(result.config->create);
    EXPECT_EQ(result.config->game, folder);
}

TEST_F(EditorCli, NewAcceptsAnEmptyFolder) {
    const auto folder = _game.path() / "empty";
    std::filesystem::create_directories(folder);

    EXPECT_TRUE(parse_cli(args({folder.string(), "--new"})).config.has_value());
}

TEST_F(EditorCli, NewRefusesAFolderThatIsNotEmpty) {
    const auto result = parse_cli(args({"--new", _game.path().string()}));

    EXPECT_FALSE(result.config.has_value());
    EXPECT_EQ(result.exit_code, error_exit_code);
    EXPECT_NE(result.message.find("not empty"), std::string::npos) << result.message;
}

TEST_F(EditorCli, NewRefusesAFile) {
    expect_rejected(args({"--new", (_game.path() / "game.json").string()}));
}

TEST_F(EditorCli, AFolderWithoutGameJsonPointsAtNew) {
    const auto result = parse_cli(args({(_game.path() / "scenes").string()}));

    EXPECT_NE(result.message.find("--new"), std::string::npos) << result.message;
}

TEST_F(EditorCli, OpeningAGameDoesNotCreate) {
    const auto result = parse_cli(args({_game.path().string()}));

    ASSERT_TRUE(result.config.has_value());
    EXPECT_FALSE(result.config->create);
}
