#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "Cli.hpp"

using rtype::server::parse_cli;
using rtype::server::ServerConfig;

namespace {

constexpr int error_exit_code = 84;

std::vector<std::string> args(std::initializer_list<std::string> rest) {
    std::vector<std::string> all{"r-type_server"};
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

TEST(ServerCli, AcceptsValidPort) {
    const auto result = parse_cli(args({"4242"}));

    EXPECT_EQ(result.config, ServerConfig{.port = 4242});
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_TRUE(result.message.empty());
}

TEST(ServerCli, AcceptsPortBounds) {
    EXPECT_EQ(parse_cli(args({"1"})).config, ServerConfig{.port = 1});
    EXPECT_EQ(parse_cli(args({"65535"})).config, ServerConfig{.port = 65535});
}

TEST(ServerCli, DefaultsToFourPlayers) {
    const auto result = parse_cli(args({"4242"}));

    ASSERT_TRUE(result.config.has_value());
    EXPECT_EQ(result.config->max_players, 4U);
}

TEST(ServerCli, AcceptsMaxPlayersWithShortAndLongFlag) {
    EXPECT_EQ(parse_cli(args({"4242", "--max-players", "2"})).config,
              (ServerConfig{.port = 4242, .max_players = 2}));
    EXPECT_EQ(parse_cli(args({"-p", "1", "4242"})).config,
              (ServerConfig{.port = 4242, .max_players = 1}));
}

TEST(ServerCli, RejectsMaxPlayersOutOfRange) {
    expect_rejected(args({"4242", "--max-players", "0"}));
    expect_rejected(args({"4242", "--max-players", "5"}));
    expect_rejected(args({"4242", "--max-players", "-1"}));
    expect_rejected(args({"4242", "--max-players", "two"}));
    expect_rejected(args({"4242", "--max-players"}));
}

TEST(ServerCli, PrintsHelpWithShortAndLongFlag) {
    for (const std::string flag : {"-h", "--help"}) {
        const auto result = parse_cli(args({flag}));

        EXPECT_FALSE(result.config.has_value()) << flag;
        EXPECT_EQ(result.exit_code, 0) << flag;
        EXPECT_NE(result.message.find("port"), std::string::npos) << flag;
    }
}

TEST(ServerCli, HelpWinsOverInvalidPort) {
    const auto result = parse_cli(args({"abc", "--help"}));

    EXPECT_EQ(result.exit_code, 0);
}

TEST(ServerCli, RejectsMissingPort) { expect_rejected(args({})); }

TEST(ServerCli, RejectsNonNumericPort) { expect_rejected(args({"abc"})); }

TEST(ServerCli, RejectsPortZero) { expect_rejected(args({"0"})); }

TEST(ServerCli, RejectsPortAboveRange) { expect_rejected(args({"65536"})); }

TEST(ServerCli, RejectsNegativePort) { expect_rejected(args({"-1"})); }

TEST(ServerCli, RejectsExtraArgument) { expect_rejected(args({"4242", "extra"})); }

TEST(ServerCli, RejectsUnknownFlag) { expect_rejected(args({"4242", "--verbose"})); }

TEST(ServerCli, RejectsEmptyArgumentList) { expect_rejected({}); }
