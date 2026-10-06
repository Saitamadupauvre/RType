#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "Cli.hpp"

using rtype::client::parse_cli;

namespace {

constexpr int error_exit_code = 84;

std::vector<std::string> args(std::initializer_list<std::string> rest) {
    std::vector<std::string> all{"r-type_client"};
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

TEST(ClientCli, AcceptsHostAndPort) {
    const auto result = parse_cli(args({"127.0.0.1", "4242"}));

    ASSERT_TRUE(result.config.has_value());
    EXPECT_EQ(result.config->host, "127.0.0.1");
    EXPECT_EQ(result.config->port, 4242);
    EXPECT_EQ(result.exit_code, 0);
    EXPECT_TRUE(result.message.empty());
}

TEST(ClientCli, KeepsHostNameUnresolved) {
    const auto result = parse_cli(args({"localhost", "4242"}));

    ASSERT_TRUE(result.config.has_value());
    EXPECT_EQ(result.config->host, "localhost");
}

TEST(ClientCli, AcceptsPortBounds) {
    EXPECT_EQ(parse_cli(args({"127.0.0.1", "1"})).config->port, 1);
    EXPECT_EQ(parse_cli(args({"127.0.0.1", "65535"})).config->port, 65535);
}

TEST(ClientCli, PrintsHelpWithShortAndLongFlag) {
    for (const std::string flag : {"-h", "--help"}) {
        const auto result = parse_cli(args({flag}));

        EXPECT_FALSE(result.config.has_value()) << flag;
        EXPECT_EQ(result.exit_code, 0) << flag;
        EXPECT_NE(result.message.find("server-ip"), std::string::npos) << flag;
    }
}

TEST(ClientCli, HelpWinsOverMissingArguments) {
    const auto result = parse_cli(args({"--help"}));

    EXPECT_EQ(result.exit_code, 0);
}

TEST(ClientCli, RejectsMissingArguments) { expect_rejected(args({})); }

TEST(ClientCli, RejectsMissingPort) { expect_rejected(args({"127.0.0.1"})); }

TEST(ClientCli, RejectsEmptyHost) { expect_rejected(args({"", "4242"})); }

TEST(ClientCli, RejectsNonNumericPort) { expect_rejected(args({"127.0.0.1", "abc"})); }

TEST(ClientCli, RejectsPortZero) { expect_rejected(args({"127.0.0.1", "0"})); }

TEST(ClientCli, RejectsPortAboveRange) { expect_rejected(args({"127.0.0.1", "65536"})); }

TEST(ClientCli, RejectsSwappedArguments) { expect_rejected(args({"4242", "127.0.0.1"})); }

TEST(ClientCli, RejectsExtraArgument) { expect_rejected(args({"127.0.0.1", "4242", "extra"})); }

TEST(ClientCli, RejectsUnknownFlag) { expect_rejected(args({"127.0.0.1", "4242", "--verbose"})); }

TEST(ClientCli, RejectsEmptyArgumentList) { expect_rejected({}); }
