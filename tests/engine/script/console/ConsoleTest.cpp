#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "engine/core/console/CVars.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"
#include "engine/script/ScriptRuntime.hpp"

namespace logger = engine::core::log;
using engine::core::console::CVarKind;
using engine::core::console::CVarRange;
using engine::core::console::CVars;
using engine::core::console::CVarValue;
using engine::script::ScriptRuntime;

namespace {

std::filesystem::path console_fixture() {
    return std::filesystem::path(RTYPE_FIXTURES_DIR) / "scripts" / "console";
}

class ScriptConsoleTest : public ::testing::Test {
protected:
    ScriptConsoleTest() { _runtime.attach_cvars(_cvars); }

    [[nodiscard]] std::size_t errors() const {
        std::size_t count = 0;
        for (const auto& record : _sink->records()) {
            count += record.level == logger::Level::Error ? 1U : 0U;
        }
        return count;
    }

    std::shared_ptr<logger::MemorySink> _sink = std::make_shared<logger::MemorySink>();
    logger::ScopedSink _scope{_sink, logger::Level::Trace};
    CVars _cvars;
    ScriptRuntime _runtime = ScriptRuntime::builder().build();
};

} // namespace

TEST_F(ScriptConsoleTest, ReturnsTheValueOfAnExpression) {
    const auto result = _runtime.evaluate("1 + 1, 'two', nil");
    EXPECT_TRUE(result.ok);
    EXPECT_EQ(result.output, "2\ttwo\tnil");
}

TEST_F(ScriptConsoleTest, RunsStatementsAndKeepsGlobals) {
    EXPECT_TRUE(_runtime.evaluate("x = 41").ok);
    EXPECT_EQ(_runtime.evaluate("x + 1").output, "42");
    EXPECT_TRUE(_runtime.evaluate("local y = 1").output.empty());
}

TEST_F(ScriptConsoleTest, CapturesPrint) {
    const auto result = _runtime.evaluate("print('a', 1, true) print('b') return 3");
    EXPECT_TRUE(result.ok);
    EXPECT_EQ(result.output, "a\t1\ttrue\nb\n3");
}

TEST_F(ScriptConsoleTest, ReturnsSyntaxAndRuntimeErrorsWithoutThrowing) {
    const auto syntax = _runtime.evaluate("x = = 1");
    EXPECT_FALSE(syntax.ok);
    EXPECT_NE(syntax.output.find("console:1:"), std::string::npos) << syntax.output;

    const auto runtime = _runtime.evaluate("print('before') error('boom')");
    EXPECT_FALSE(runtime.ok);
    EXPECT_NE(runtime.output.find("before"), std::string::npos);
    EXPECT_NE(runtime.output.find("boom"), std::string::npos);

    EXPECT_EQ(_runtime.evaluate("2 * 3").output, "6");
}

TEST_F(ScriptConsoleTest, ReadsAndWritesTheGlobalsOfMainLua) {
    ASSERT_TRUE(_runtime.load_scripts(console_fixture()));

    EXPECT_EQ(_runtime.evaluate("score").output, "10");
    EXPECT_TRUE(_runtime.evaluate("score = 99").ok);
    EXPECT_EQ(_runtime.evaluate("score").output, "99");
    EXPECT_EQ(_runtime.evaluate("lives()").output, "3");
}

TEST_F(ScriptConsoleTest, ScriptsRegisterVariablesAtLoadTime) {
    ASSERT_TRUE(_runtime.load_scripts(console_fixture()));

    const auto lives = _cvars.find("game.lives");
    ASSERT_TRUE(lives.has_value());
    EXPECT_EQ(lives->kind, CVarKind::integer);
    EXPECT_EQ(lives->description, "lives at start");
    ASSERT_TRUE(lives->range.has_value());
    EXPECT_DOUBLE_EQ(lives->range->max, 9.0);
    const auto speed = _cvars.find("game.speed");
    ASSERT_TRUE(speed.has_value());
    EXPECT_EQ(speed.value_or(engine::core::console::CVarInfo{}).kind, CVarKind::number);
    EXPECT_EQ(errors(), 0U);
}

TEST_F(ScriptConsoleTest, ScriptsGetSetResetAndListVariables) {
    ASSERT_TRUE(_cvars.add({.name = "audio.volume",
                            .default_value = 0.5,
                            .description = "",
                            .range = CVarRange{.min = 0.0, .max = 1.0}}));
    ASSERT_TRUE(_cvars.add({.name = "debug.hitboxes", .default_value = false}));

    EXPECT_EQ(_runtime.evaluate("cvar.get('audio.volume')").output, "0.5");
    EXPECT_EQ(_runtime.evaluate("cvar.set('debug.hitboxes', true)").output, "true");
    EXPECT_EQ(_cvars.get("debug.hitboxes"), CVarValue(true));
    EXPECT_EQ(_runtime.evaluate("cvar.set('audio.volume', 1)").output, "true");
    EXPECT_EQ(_cvars.get("audio.volume"), CVarValue(1.0));
    EXPECT_EQ(_runtime.evaluate("cvar.reset('audio.volume')").output, "true");
    EXPECT_EQ(_cvars.get("audio.volume"), CVarValue(0.5));
    EXPECT_EQ(_runtime.evaluate("table.concat(cvar.list(), ',')").output,
              "audio.volume,debug.hitboxes");
}

TEST_F(ScriptConsoleTest, BadCallsAreLoggedAndReturnFalse) {
    ASSERT_TRUE(_cvars.add({.name = "debug.hitboxes", .default_value = false}));

    for (const auto* line :
         {"cvar.set('missing', 1)", "cvar.set('debug.hitboxes', 1)",
          "cvar.set('debug.hitboxes', {})", "cvar.set(3, true)", "cvar.reset('missing')",
          "cvar.register('x')", "cvar.register{name = 'debug.hitboxes', default = true}",
          "cvar.register{name = 'bad name', default = true}",
          "cvar.register{name = 'x', default = {}}",
          "cvar.register{name = 'x', default = 1, min = 0}",
          "cvar.register{name = 'x', default = 'a', kind = 'int'}",
          "cvar.register{name = 'x', default = 5, min = 0, max = 1}"}) {
        const auto result = _runtime.evaluate(line);
        EXPECT_TRUE(result.ok) << line;
        EXPECT_EQ(result.output, "false") << line;
    }
    EXPECT_EQ(_runtime.evaluate("cvar.get('missing')").output, "nil");
    EXPECT_EQ(errors(), 13U);
    EXPECT_EQ(_cvars.list().size(), 1U);
}

TEST_F(ScriptConsoleTest, IntegerKindAcceptsWholeNumbers) {
    EXPECT_EQ(
        _runtime.evaluate("cvar.register{name = 'sv.rate', default = 60.0, kind = 'int'}").output,
        "true");
    EXPECT_EQ(_cvars.get("sv.rate"), CVarValue(std::int64_t{60}));
}

TEST(ScriptConsole, CvarIsMissingWithoutARegistry) {
    ScriptRuntime runtime = ScriptRuntime::builder().build();
    const auto result = runtime.evaluate("cvar.get('x')");
    EXPECT_FALSE(result.ok);
}
