#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "Cli.hpp"

using engine::app::Mode;
using runtime::parse_cli;
using runtime::RuntimeConfig;

namespace {

constexpr int error_exit_code = 84;

std::vector<std::string> args(std::initializer_list<std::string> rest) {
    std::vector<std::string> all{"engine-runtime"};
    all.insert(all.end(), rest);
    return all;
}

void expect_rejected(const std::vector<std::string>& arguments) {
    const auto result = parse_cli(arguments);

    EXPECT_FALSE(result.config.has_value());
    EXPECT_EQ(result.exit_code, error_exit_code);
    EXPECT_FALSE(result.message.empty());
}

} // namespace

TEST(RuntimeCli, JoiningAServerIsTheClientMode) {
    const auto result = parse_cli(args({"127.0.0.1", "4242"}));

    EXPECT_EQ(result.config, (RuntimeConfig{.server_ip = "127.0.0.1", .port = 4242}));
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_TRUE(result.message.empty());
}

TEST(RuntimeCli, KeepsHostNameUnresolved) {
    const auto result = parse_cli(args({"localhost", "4242"}));

    EXPECT_EQ(result.config, (RuntimeConfig{.server_ip = "localhost", .port = 4242}));
}

TEST(RuntimeCli, AcceptsPortBounds) {
    EXPECT_EQ(parse_cli(args({"127.0.0.1", "1"})).config,
              (RuntimeConfig{.server_ip = "127.0.0.1", .port = 1}));
    EXPECT_EQ(parse_cli(args({"127.0.0.1", "65535"})).config,
              (RuntimeConfig{.server_ip = "127.0.0.1", .port = 65535}));
}

TEST(RuntimeCli, HostTakesThePortToListenOn) {
    const auto result = parse_cli(args({"--host", "4242", "--mute"}));

    EXPECT_EQ(result.config, (RuntimeConfig{.mode = Mode::host, .port = 4242, .muted = true}));
}

TEST(RuntimeCli, ServerRunsTheDedicatedMode) {
    const auto result = parse_cli(args({"--server", "4242"}));

    EXPECT_EQ(result.config, (RuntimeConfig{.mode = Mode::dedicated, .port = 4242}));
}

TEST(RuntimeCli, SoloNeedsNoAddress) {
    const auto result = parse_cli(args({"--solo"}));

    EXPECT_EQ(result.config, (RuntimeConfig{.mode = Mode::solo}));
}

TEST(RuntimeCli, DefaultsToFourPlayersFullVolumeUnmuted) {
    const auto result = parse_cli(args({"--solo"}));

    ASSERT_TRUE(result.config.has_value());
    EXPECT_EQ(result.config->max_players, 4U);
    EXPECT_EQ(result.config->music_volume, 100U);
    EXPECT_EQ(result.config->effects_volume, 100U);
    EXPECT_FALSE(result.config->muted);
}

TEST(RuntimeCli, AcceptsMaxPlayersWithShortAndLongFlag) {
    EXPECT_EQ(parse_cli(args({"--server", "4242", "--max-players", "2"})).config,
              (RuntimeConfig{.mode = Mode::dedicated, .port = 4242, .max_players = 2}));
    EXPECT_EQ(parse_cli(args({"-p", "1", "--host", "4242"})).config,
              (RuntimeConfig{.mode = Mode::host, .port = 4242, .max_players = 1}));
}

TEST(RuntimeCli, RejectsMaxPlayersOutOfRange) {
    expect_rejected(args({"--server", "4242", "--max-players", "0"}));
    expect_rejected(args({"--server", "4242", "--max-players", "5"}));
    expect_rejected(args({"--server", "4242", "--max-players", "two"}));
    expect_rejected(args({"--server", "4242", "--max-players"}));
}

TEST(RuntimeCli, AcceptsVolumesAndMute) {
    const auto result = parse_cli(
        args({"127.0.0.1", "4242", "--music-volume", "30", "--effects-volume", "0", "--mute"}));

    EXPECT_EQ(result.config, (RuntimeConfig{.server_ip = "127.0.0.1",
                                            .port = 4242,
                                            .music_volume = 30,
                                            .effects_volume = 0,
                                            .muted = true}));
}

TEST(RuntimeCli, RejectsVolumesOutOfRange) {
    expect_rejected(args({"--solo", "--music-volume", "101"}));
    expect_rejected(args({"--solo", "--effects-volume", "-1"}));
    expect_rejected(args({"--solo", "--music-volume", "loud"}));
}

TEST(RuntimeCli, AcceptsAGameFolderAndASceneFile) {
    const auto result =
        parse_cli(args({"--solo", "--game", "../my-game", "--scene-file", "/tmp/level.json"}));

    ASSERT_TRUE(result.config.has_value());
    EXPECT_EQ(result.config->game, "../my-game");
    EXPECT_EQ(result.config->scene_file, "/tmp/level.json");
}

TEST(RuntimeCli, GameAndSceneFileDefaultToEmpty) {
    const auto result = parse_cli(args({"--solo"}));

    ASSERT_TRUE(result.config.has_value());
    EXPECT_TRUE(result.config->game.empty());
    EXPECT_TRUE(result.config->scene_file.empty());
}

TEST(RuntimeCli, PrintsHelpWithShortAndLongFlag) {
    for (const std::string flag : {"-h", "--help"}) {
        const auto result = parse_cli(args({flag}));

        EXPECT_FALSE(result.config.has_value()) << flag;
        EXPECT_EQ(result.exit_code, 0) << flag;
        EXPECT_NE(result.message.find("--server"), std::string::npos) << flag;
    }
}

TEST(RuntimeCli, HelpWinsOverInvalidArguments) {
    EXPECT_EQ(parse_cli(args({"abc", "--help"})).exit_code, 0);
}

TEST(RuntimeCli, RejectsNoMode) { expect_rejected(args({})); }

TEST(RuntimeCli, RejectsEmptyArgumentList) { expect_rejected({}); }

TEST(RuntimeCli, RejectsSeveralModesAtOnce) {
    expect_rejected(args({"127.0.0.1", "4242", "--solo"}));
    expect_rejected(args({"127.0.0.1", "4242", "--host", "4243"}));
    expect_rejected(args({"--host", "4242", "--solo"}));
    expect_rejected(args({"--server", "4242", "--solo"}));
    expect_rejected(args({"--server", "4242", "--host", "4243"}));
}

TEST(RuntimeCli, RejectsMissingPort) { expect_rejected(args({"127.0.0.1"})); }

TEST(RuntimeCli, RejectsEmptyServerAddress) { expect_rejected(args({"", "4242"})); }

TEST(RuntimeCli, RejectsInvalidPorts) {
    expect_rejected(args({"127.0.0.1", "abc"}));
    expect_rejected(args({"127.0.0.1", "0"}));
    expect_rejected(args({"127.0.0.1", "65536"}));
    expect_rejected(args({"--host"}));
    expect_rejected(args({"--host", "0"}));
    expect_rejected(args({"--server", "70000"}));
}

TEST(RuntimeCli, RejectsSwappedArguments) { expect_rejected(args({"4242", "127.0.0.1"})); }

TEST(RuntimeCli, RejectsExtraArgument) { expect_rejected(args({"127.0.0.1", "4242", "extra"})); }

TEST(RuntimeCli, RejectsUnknownFlag) { expect_rejected(args({"--solo", "--verbose"})); }

TEST(RuntimeCli, RejectsAnEmptySceneFile) { expect_rejected(args({"--solo", "--scene-file", ""})); }
