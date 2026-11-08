#include <gtest/gtest.h>

#include <initializer_list>
#include <string>
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
