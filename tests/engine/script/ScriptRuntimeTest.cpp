#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"
#include "engine/script/ScriptRuntime.hpp"

namespace logger = engine::core::log;
using engine::script::ScriptRuntime;

namespace {

std::filesystem::path script_fixture(const std::string& name) {
    return std::filesystem::path(RTYPE_FIXTURES_DIR) / "scripts" / name;
}

class ScriptRuntimeTest : public ::testing::Test {
protected:
    std::shared_ptr<logger::MemorySink> _sink = std::make_shared<logger::MemorySink>();
    logger::ScopedSink _scope{_sink, logger::Level::Trace};
    ScriptRuntime _runtime;

    [[nodiscard]] std::vector<logger::MemoryRecord> errors() const {
        std::vector<logger::MemoryRecord> result;
        for (const auto& record : _sink->records()) {
            if (record.level == logger::Level::Error) {
                result.push_back(record);
            }
        }
        return result;
    }
};

} // namespace

TEST_F(ScriptRuntimeTest, RunsValidString) {
    EXPECT_TRUE(_runtime.run_string("local x = 1 + 2 assert(x == 3)", "inline"));
    EXPECT_TRUE(errors().empty());
}

TEST_F(ScriptRuntimeTest, RunsValidFile) {
    EXPECT_TRUE(_runtime.run_file(script_fixture("valid.lua")));
    EXPECT_TRUE(_runtime.run_string("assert(counter == 1)", "check"));
    EXPECT_TRUE(errors().empty());
}

TEST_F(ScriptRuntimeTest, KeepsGlobalsBetweenRuns) {
    ASSERT_TRUE(_runtime.run_string("score = 41", "first"));

    EXPECT_TRUE(_runtime.run_string("score = score + 1 assert(score == 42)", "second"));
}

TEST_F(ScriptRuntimeTest, OpensSafeLibraries) {
    EXPECT_TRUE(_runtime.run_string(R"(
        assert(type(print) == "function")
        assert(math.floor(2.5) == 2)
        assert(string.upper("a") == "A")
        assert(table.concat({"a", "b"}) == "ab")
        assert(type(coroutine.create) == "function")
    )",
                                    "libraries"));
    EXPECT_TRUE(errors().empty());
}

TEST_F(ScriptRuntimeTest, IoLibraryIsUnavailable) {
    EXPECT_TRUE(_runtime.run_string("assert(io == nil)", "sandbox"));
}

TEST_F(ScriptRuntimeTest, OsLibraryIsUnavailable) {
    EXPECT_TRUE(_runtime.run_string("assert(os == nil)", "sandbox"));
}

TEST_F(ScriptRuntimeTest, DofileAndLoadfileAreUnavailable) {
    EXPECT_TRUE(_runtime.run_string("assert(dofile == nil and loadfile == nil)", "sandbox"));
}

TEST_F(ScriptRuntimeTest, PackageLibraryIsUnavailable) {
    EXPECT_TRUE(_runtime.run_string("assert(package == nil)", "sandbox"));
}

TEST_F(ScriptRuntimeTest, CallingIoFailsWithoutCrashing) {
    EXPECT_FALSE(_runtime.run_string("io.open('/etc/passwd')", "escape"));
    ASSERT_EQ(errors().size(), 1U);
}

TEST_F(ScriptRuntimeTest, RuntimeErrorInStringIsLoggedWithChunkAndLine) {
    EXPECT_FALSE(_runtime.run_string("local a = 1\nerror('boom')", "bydo"));

    const auto logged = errors();
    ASSERT_EQ(logged.size(), 1U);
    EXPECT_NE(logged.front().message.find("bydo:2:"), std::string::npos);
    EXPECT_NE(logged.front().message.find("boom"), std::string::npos);
}

TEST_F(ScriptRuntimeTest, RuntimeErrorInFileIsLoggedWithFileAndLine) {
    const auto path = script_fixture("runtime_error.lua");

    EXPECT_FALSE(_runtime.run_file(path));

    const auto logged = errors();
    ASSERT_EQ(logged.size(), 1U);
    EXPECT_NE(logged.front().message.find("runtime_error.lua:3:"), std::string::npos);
}

TEST_F(ScriptRuntimeTest, SyntaxErrorInFileReportsFilePath) {
    EXPECT_FALSE(_runtime.run_file(script_fixture("syntax_error.lua")));

    const auto logged = errors();
    ASSERT_EQ(logged.size(), 1U);
    EXPECT_NE(logged.front().message.find("syntax_error.lua:1:"), std::string::npos);
}

TEST_F(ScriptRuntimeTest, SyntaxErrorInStringReportsChunkName) {
    EXPECT_FALSE(_runtime.run_string("local = 1", "broken"));

    const auto logged = errors();
    ASSERT_EQ(logged.size(), 1U);
    EXPECT_NE(logged.front().message.find("broken:1:"), std::string::npos);
}

TEST_F(ScriptRuntimeTest, MissingFileFailsWithoutThrowing) {
    EXPECT_FALSE(_runtime.run_file(script_fixture("does_not_exist.lua")));

    const auto logged = errors();
    ASSERT_EQ(logged.size(), 1U);
    EXPECT_NE(logged.front().message.find("does_not_exist.lua"), std::string::npos);
}

TEST_F(ScriptRuntimeTest, KeepsRunningAfterError) {
    ASSERT_FALSE(_runtime.run_string("error('first')", "failing"));

    EXPECT_TRUE(_runtime.run_string("value = 7 assert(value == 7)", "recovering"));
    EXPECT_EQ(errors().size(), 1U);
}

TEST_F(ScriptRuntimeTest, RunStringRejectsBytecode) {
    ASSERT_TRUE(_runtime.run_string("bytecode = string.dump(function() end)", "dump"));

    EXPECT_FALSE(_runtime.run_string("assert(load(bytecode) == nil)\nerror('loaded')", "probe"));
    const auto logged = errors();
    ASSERT_EQ(logged.size(), 1U);
    EXPECT_NE(logged.front().message.find("probe:2:"), std::string::npos);
}

TEST_F(ScriptRuntimeTest, LoadRejectsBytecodeEvenWhenAskedFor) {
    EXPECT_TRUE(_runtime.run_string(R"(
        local fn, err = load(string.dump(function() end), "x", "b")
        assert(fn == nil and err ~= nil)
    )",
                                    "sandbox"));
}

TEST_F(ScriptRuntimeTest, LoadStillCompilesText) {
    EXPECT_TRUE(_runtime.run_string("assert(load('return 5')() == 5)", "sandbox"));
}
