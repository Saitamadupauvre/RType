#include <gtest/gtest.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "engine/core/console/CVars.hpp"
#include "engine/core/console/Console.hpp"

using engine::core::console::Console;
using engine::core::console::CVarRange;
using engine::core::console::CVars;
using engine::core::console::CVarValue;
using engine::core::console::ExecutionResult;
using engine::core::console::LineKind;

namespace {

class ConsoleTest : public ::testing::Test {
protected:
    ConsoleTest() {
        static_cast<void>(cvars.add({.name = "audio.volume",
                                     .default_value = 0.5,
                                     .description = "master volume",
                                     .range = CVarRange{.min = 0.0, .max = 1.0}}));
        static_cast<void>(cvars.add({.name = "debug.hitboxes", .default_value = false}));
    }

    void submit(std::string_view line) {
        console.type(line);
        console.submit();
    }

    [[nodiscard]] std::vector<std::string> texts(LineKind kind) const {
        std::vector<std::string> found;
        for (const auto& line : console.lines()) {
            if (line.kind == kind) {
                found.push_back(line.text);
            }
        }
        return found;
    }

    [[nodiscard]] bool printed(LineKind kind, std::string_view text) const {
        return std::ranges::any_of(texts(kind), [text](const std::string& line) {
            return line.find(text) != std::string::npos;
        });
    }

    CVars cvars;
    Console console{cvars};
};

} // namespace

TEST_F(ConsoleTest, StartsClosedAndEmpty) {
    EXPECT_FALSE(console.is_open());
    EXPECT_TRUE(console.input().empty());
    EXPECT_EQ(console.cursor(), 0U);
    EXPECT_TRUE(console.lines().empty());
    console.set_open(true);
    EXPECT_TRUE(console.is_open());
}

TEST_F(ConsoleTest, TypesAtTheCursorAndDropsControlCharacters) {
    console.type("pint(1)");
    console.move_home();
    console.move_right();
    console.type(U'r');
    console.type("\t\n\x7f");
    EXPECT_EQ(console.input(), "print(1)");
    EXPECT_EQ(console.cursor(), 2U);
}

TEST_F(ConsoleTest, EditsMultiByteCharactersAsOneCharacter) {
    console.type(U'é');
    console.type(U'€');
    console.type(U'😀');
    EXPECT_EQ(console.input(), "é€😀");
    console.move_left();
    EXPECT_EQ(console.cursor(), std::string("é€").size());
    console.erase_before();
    EXPECT_EQ(console.input(), "é😀");
    console.move_home();
    console.erase_after();
    EXPECT_EQ(console.input(), "😀");
    console.move_end();
    console.erase_before();
    EXPECT_TRUE(console.input().empty());
}

TEST_F(ConsoleTest, EditingAtTheEdgesDoesNothing) {
    console.erase_before();
    console.erase_after();
    console.move_left();
    console.move_right();
    console.type("ab");
    console.erase_after();
    console.move_home();
    console.erase_before();
    EXPECT_EQ(console.input(), "ab");
    EXPECT_EQ(console.cursor(), 0U);
}

TEST_F(ConsoleTest, SubmitEchoesRunsAndClearsTheInput) {
    std::vector<std::string> ran;
    console.set_lua([&ran](std::string_view code) {
        ran.emplace_back(code);
        return ExecutionResult{.ok = true, .output = "2"};
    });

    submit("  1 + 1 ");

    EXPECT_EQ(ran, std::vector<std::string>{"1 + 1"});
    EXPECT_TRUE(console.input().empty());
    EXPECT_EQ(texts(LineKind::input), std::vector<std::string>{">   1 + 1 "});
    EXPECT_EQ(texts(LineKind::output), std::vector<std::string>{"2"});
}

TEST_F(ConsoleTest, PrintsLuaErrorsAndKeepsRunning) {
    console.set_lua([](std::string_view code) {
        if (code == "boom") {
            return ExecutionResult{.ok = false, .output = "console:1: boom"};
        }
        return ExecutionResult{.ok = true, .output = "fine"};
    });

    submit("boom");
    submit("ok");

    EXPECT_TRUE(printed(LineKind::error, "console:1: boom"));
    EXPECT_TRUE(printed(LineKind::output, "fine"));
}

TEST_F(ConsoleTest, TurnsAThrowingExecutorIntoAnErrorLine) {
    console.set_lua(
        [](std::string_view) -> ExecutionResult { throw std::runtime_error("broken"); });
    submit("x");
    EXPECT_TRUE(printed(LineKind::error, "broken"));
}

TEST_F(ConsoleTest, ReportsAMissingRuntime) {
    submit("print(1)");
    submit("/server print(1)");
    EXPECT_TRUE(printed(LineKind::error, "no Lua runtime"));
    EXPECT_TRUE(printed(LineKind::error, "no local server"));
}

TEST_F(ConsoleTest, SendsServerLinesToTheServerExecutor) {
    std::vector<std::string> client;
    std::vector<std::string> server;
    console.set_lua([&client](std::string_view code) {
        client.emplace_back(code);
        return ExecutionResult{};
    });
    console.set_server_lua([&server](std::string_view code) {
        server.emplace_back(code);
        return ExecutionResult{};
    });

    submit("/server spawn('enemy')");
    submit("/server");
    submit("/serverless()");

    EXPECT_EQ(server, std::vector<std::string>{"spawn('enemy')"});
    EXPECT_EQ(client, std::vector<std::string>{"/serverless()"});
    EXPECT_TRUE(printed(LineKind::error, "nothing to run"));
}

TEST_F(ConsoleTest, IgnoresBlankLines) {
    submit("   ");
    EXPECT_TRUE(console.lines().empty());
    EXPECT_TRUE(console.history().empty());
}

TEST_F(ConsoleTest, HelpListsTheCommands) {
    submit("help");
    EXPECT_TRUE(printed(LineKind::output, "clear"));
    EXPECT_TRUE(printed(LineKind::output, "cvar"));
    EXPECT_TRUE(printed(LineKind::output, "/server"));
}

TEST_F(ConsoleTest, ClearEmptiesTheScrollback) {
    console.print(LineKind::log, "old");
    submit("clear");
    EXPECT_TRUE(console.lines().empty());
}

TEST_F(ConsoleTest, CvarListsShowsAndSetsVariables) {
    submit("cvar");
    EXPECT_TRUE(printed(LineKind::output, "audio.volume = 0.5  master volume"));
    EXPECT_TRUE(printed(LineKind::output, "debug.hitboxes = false"));

    console.clear();
    submit("cvar list hit");
    EXPECT_EQ(texts(LineKind::output).size(), 1U);

    submit("cvar audio.volume");
    EXPECT_TRUE(printed(LineKind::output, "audio.volume = 0.5 (number, default 0.5, 0 to 1)"));

    submit("cvar debug.hitboxes on");
    EXPECT_TRUE(printed(LineKind::output, "debug.hitboxes = true"));
    EXPECT_EQ(cvars.get("debug.hitboxes"), CVarValue(true));

    submit("cvar reset debug.hitboxes");
    EXPECT_EQ(cvars.get("debug.hitboxes"), CVarValue(false));
}

TEST_F(ConsoleTest, CvarPrintsErrors) {
    submit("cvar missing");
    submit("cvar missing 1");
    submit("cvar audio.volume 3");
    submit("cvar debug.hitboxes maybe");
    submit("cvar list nothing_matches");

    EXPECT_EQ(texts(LineKind::error).size(), 4U);
    EXPECT_TRUE(printed(LineKind::error, "unknown variable missing"));
    EXPECT_TRUE(printed(LineKind::error, "between 0 and 1"));
    EXPECT_TRUE(printed(LineKind::output, "no variable"));
    EXPECT_EQ(cvars.get("audio.volume"), CVarValue(0.5));
}

TEST_F(ConsoleTest, HistoryRecallsSubmittedLinesAndTheDraft) {
    console.set_lua([](std::string_view) { return ExecutionResult{}; });
    submit("first");
    submit("second");
    submit("second");
    EXPECT_EQ(console.history(), (std::vector<std::string>{"first", "second"}));

    console.type("draft");
    console.history_previous();
    EXPECT_EQ(console.input(), "second");
    EXPECT_EQ(console.cursor(), console.input().size());
    console.history_previous();
    console.history_previous();
    EXPECT_EQ(console.input(), "first");
    console.history_next();
    EXPECT_EQ(console.input(), "second");
    console.history_next();
    EXPECT_EQ(console.input(), "draft");
    console.history_next();
    EXPECT_EQ(console.input(), "draft");
}

TEST_F(ConsoleTest, HistoryAndScrollbackAreBounded) {
    Console small(cvars, 3, 2);
    small.set_lua([](std::string_view) { return ExecutionResult{}; });
    for (const auto* line : {"a", "b", "c"}) {
        small.type(line);
        small.submit();
    }
    small.print(LineKind::log, "one\ntwo");

    EXPECT_EQ(small.history(), (std::vector<std::string>{"b", "c"}));
    ASSERT_EQ(small.lines().size(), 3U);
    EXPECT_EQ(small.lines().front().text, "> c");
    EXPECT_EQ(small.lines().back().text, "two");
}

TEST_F(ConsoleTest, ScrollStaysWithinTheScrollback) {
    for (int line = 0; line < 5; ++line) {
        console.print(LineKind::log, std::to_string(line));
    }
    console.scroll(2);
    EXPECT_EQ(console.scroll_offset(), 2U);
    console.scroll(100);
    EXPECT_EQ(console.scroll_offset(), 4U);
    console.scroll(-100);
    EXPECT_EQ(console.scroll_offset(), 0U);
    console.scroll(3);
    console.clear();
    EXPECT_EQ(console.scroll_offset(), 0U);
}
