#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

#include "../../engine/script/scene/TempFolder.hpp"
#include "open/SystemOpener.hpp"

using editor::SystemOpener;

namespace {

constexpr long max_polls = 10'000'000;

std::filesystem::path fixture(const char* name) {
    return std::filesystem::path(TEST_FIXTURES_DIR) / "editor" / name;
}

std::vector<std::string> wait_for_failures(SystemOpener& opener) {
    for (long poll = 0; poll < max_polls; ++poll) {
        if (auto failures = opener.take_failures(); !failures.empty()) {
            return failures;
        }
        std::this_thread::yield();
    }
    return {};
}

bool wait_for_file(const std::filesystem::path& file) {
    for (long poll = 0; poll < max_polls; ++poll) {
        if (std::filesystem::exists(file)) {
            return true;
        }
        std::this_thread::yield();
    }
    return false;
}

std::string read_file(const std::filesystem::path& file) {
    std::ifstream stream(file);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

} // namespace

TEST(SystemOpener, GivesTheFileAsTheOnlyArgumentWithoutATerminal) {
    const TempFolder folder;
    const std::filesystem::path file = folder.path() / "player.lua";
    SystemOpener opener(fixture("opener-records.sh"));

    ASSERT_TRUE(opener.open(file));

    const std::filesystem::path record = folder.path() / "player.lua.args";
    ASSERT_TRUE(wait_for_file(record));
    EXPECT_EQ(read_file(record), "count: 1\nfile: " + file.string() + "\nstdin: other\n");
}

TEST(SystemOpener, ReturnsWhileTheProgramStillRuns) {
    const TempFolder folder;
    const std::filesystem::path file = folder.path() / "space.fs";
    SystemOpener opener(fixture("opener-waits.sh"));

    ASSERT_TRUE(opener.open(file));
    EXPECT_TRUE(opener.take_failures().empty());

    std::ofstream(folder.path() / "space.fs.release").put('\n');
    const std::vector<std::string> failures = wait_for_failures(opener);

    ASSERT_EQ(failures.size(), 1U);
    EXPECT_NE(failures.front().find("exit code 4"), std::string::npos);
}

TEST(SystemOpener, ReportsAFailedOpenOnceWithTheFileAndCode) {
    SystemOpener opener(fixture("opener-fails.sh"));

    ASSERT_TRUE(opener.open("/games/demo/scripts/gem.lua"));
    const std::vector<std::string> failures = wait_for_failures(opener);

    ASSERT_EQ(failures.size(), 1U);
    EXPECT_NE(failures.front().find("opener-fails.sh could not open /games/demo/scripts/gem.lua"),
              std::string::npos);
    EXPECT_NE(failures.front().find("exit code 3"), std::string::npos);
    EXPECT_TRUE(opener.take_failures().empty());
}

TEST(SystemOpener, AMissingProgramCannotOpen) {
    SystemOpener opener("/no/such/opener");

    EXPECT_FALSE(opener.open("/games/demo/scripts/gem.lua"));
    EXPECT_TRUE(opener.take_failures().empty());
}

TEST(SystemOpener, UsesTheSystemOpener) {
#ifdef __APPLE__
    EXPECT_EQ(SystemOpener::default_program(), "open");
#else
    EXPECT_EQ(SystemOpener::default_program(), "xdg-open");
#endif
}
