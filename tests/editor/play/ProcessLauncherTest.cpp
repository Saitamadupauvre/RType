#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "play/ProcessLauncher.hpp"

using editor::GameRun;
using editor::PlayRequest;
using editor::ProcessLauncher;

namespace {

constexpr int signal_exit = 128;
constexpr int terminate_signal = 15;
constexpr long max_polls = 100'000'000;

std::filesystem::path fixture(const char* name) {
    return std::filesystem::path(RTYPE_FIXTURES_DIR) / "editor" / name;
}

PlayRequest request() { return {.game = "/games/demo", .scene_file = "/tmp/played.json"}; }

std::vector<std::string> run_to_end(GameRun& run) {
    for (long poll = 0; poll < max_polls && !run.exit_code(); ++poll) {
    }
    return run.take_output();
}

} // namespace

TEST(ProcessLauncher, PlaysSoloFromTheGameAndTheSceneFile) {
    ProcessLauncher launcher(fixture("fake-runtime.sh"));
    auto launch = launcher.launch(request());
    ASSERT_NE(launch.run, nullptr) << launch.error;

    const std::vector<std::string> output = run_to_end(*launch.run);

    ASSERT_FALSE(output.empty());
    EXPECT_EQ(output.front(), "args: --solo --game /games/demo --scene-file /tmp/played.json");
}

TEST(ProcessLauncher, MergesErrorsAndKeepsTheLastLineWithoutNewline) {
    ProcessLauncher launcher(fixture("fake-runtime.sh"));
    auto launch = launcher.launch(request());
    ASSERT_NE(launch.run, nullptr) << launch.error;

    const std::vector<std::string> output = run_to_end(*launch.run);

    EXPECT_EQ(output.size(), 3U);
    EXPECT_NE(std::ranges::find(output, "to stderr"), output.end());
    EXPECT_EQ(output.back(), "no newline");
}

TEST(ProcessLauncher, ReportsTheExitCode) {
    ProcessLauncher launcher(fixture("fake-runtime.sh"));
    auto launch = launcher.launch(request());
    ASSERT_NE(launch.run, nullptr) << launch.error;

    run_to_end(*launch.run);

    EXPECT_EQ(launch.run->exit_code(), 3);
}

TEST(ProcessLauncher, StopEndsARunningGame) {
    ProcessLauncher launcher(fixture("endless-runtime.sh"));
    auto launch = launcher.launch(request());
    ASSERT_NE(launch.run, nullptr) << launch.error;
    EXPECT_FALSE(launch.run->exit_code().has_value());

    launch.run->stop();

    EXPECT_EQ(launch.run->exit_code(), signal_exit + terminate_signal);
}

TEST(ProcessLauncher, PausedGameStillStops) {
    ProcessLauncher launcher(fixture("endless-runtime.sh"));
    auto launch = launcher.launch(request());
    ASSERT_NE(launch.run, nullptr) << launch.error;

    launch.run->pause();
    launch.run->stop();

    EXPECT_TRUE(launch.run->exit_code().has_value());
}

TEST(ProcessLauncher, MissingProgramIsReported) {
    ProcessLauncher launcher(fixture("no-such-runtime"));
    const auto launch = launcher.launch(request());

    EXPECT_EQ(launch.run, nullptr);
    EXPECT_NE(launch.error.find("no-such-runtime"), std::string::npos);
}
