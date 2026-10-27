#include <gtest/gtest.h>

#include <algorithm>
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

std::filesystem::path modules_fixture() {
    return std::filesystem::path(RTYPE_FIXTURES_DIR) / "scripts" / "declarations" / "modules";
}

class ScriptRequireTest : public ::testing::Test {
protected:
    std::shared_ptr<logger::MemorySink> _sink = std::make_shared<logger::MemorySink>();
    logger::ScopedSink _scope{_sink, logger::Level::Trace};
    ScriptRuntime _runtime;

    void SetUp() override { ASSERT_TRUE(_runtime.load_scripts(modules_fixture())); }

    [[nodiscard]] bool error_logged(const std::vector<std::string>& parts) const {
        return std::ranges::any_of(_sink->records(), [&](const logger::MemoryRecord& record) {
            return record.level == logger::Level::Error &&
                   std::ranges::all_of(parts, [&](const std::string& part) {
                       return record.message.find(part) != std::string::npos;
                   });
        });
    }
};

} // namespace

TEST_F(ScriptRequireTest, DoesNotLoadLibFilesAsDeclarations) {
    EXPECT_FALSE(_runtime.find_declaration("hidden"));
    EXPECT_FALSE(error_logged({"lib/"}));
}

TEST_F(ScriptRequireTest, MainCanRequireModule) {
    EXPECT_TRUE(_runtime.run_string("assert(helper_value == 42)", "check"));
}

TEST_F(ScriptRequireTest, DeclarationFileCanRequireNestedModule) {
    EXPECT_TRUE(_runtime.find_declaration("user"));
    EXPECT_TRUE(_runtime.run_string("assert(require('nested.deep').depth == 2)", "check"));
}

TEST_F(ScriptRequireTest, ReturnsSameValueOnEveryCall) {
    EXPECT_TRUE(_runtime.run_string("assert(require('helper') == require('helper'))", "check"));
}

TEST_F(ScriptRequireTest, RunsModuleOnlyOnce) {
    EXPECT_TRUE(_runtime.run_string(R"(
        require("counter")
        require("counter")
        assert(counter_runs == 1)
    )",
                                    "check"));
}

TEST_F(ScriptRequireTest, ModuleReturningNothingYieldsTrue) {
    EXPECT_TRUE(_runtime.run_string("assert(require('silent') == true)", "check"));
}

TEST_F(ScriptRequireTest, RejectsMissingModule) {
    EXPECT_FALSE(_runtime.run_string("require('ghost')", "probe"));

    EXPECT_TRUE(error_logged({"probe:1:", "module 'ghost' not found at lib/ghost.lua"}));
}

TEST_F(ScriptRequireTest, RejectsPathTraversal) {
    EXPECT_FALSE(_runtime.run_string("require('../main')", "probe"));
    EXPECT_FALSE(_runtime.run_string("require('/etc/passwd')", "probe"));
    EXPECT_FALSE(_runtime.run_string("require('..main')", "probe"));

    EXPECT_TRUE(error_logged({"invalid module name '../main'"}));
    EXPECT_TRUE(error_logged({"invalid module name '/etc/passwd'"}));
    EXPECT_TRUE(error_logged({"invalid module name '..main'"}));
}

TEST_F(ScriptRequireTest, RejectsEmptyName) {
    EXPECT_FALSE(_runtime.run_string("require('')", "probe"));

    EXPECT_TRUE(error_logged({"invalid module name ''"}));
}

TEST_F(ScriptRequireTest, ReportsErrorRaisedByModule) {
    EXPECT_FALSE(_runtime.run_string("require('broken')", "probe"));

    EXPECT_TRUE(error_logged(
        {"probe:1:", "error loading module 'broken'", "lib/broken.lua:1:", "module boom"}));
}

TEST_F(ScriptRequireTest, FailedModuleIsNotCached) {
    ASSERT_FALSE(_runtime.run_string("require('broken')", "first"));

    EXPECT_FALSE(_runtime.run_string("require('broken')", "second"));
    EXPECT_TRUE(error_logged({"second:1:", "module boom"}));
}

TEST_F(ScriptRequireTest, RejectsCircularRequire) {
    EXPECT_FALSE(_runtime.run_string("require('cycle_a')", "probe"));

    EXPECT_TRUE(error_logged({"circular require: cycle_a -> cycle_b -> cycle_a"}));
}

TEST_F(ScriptRequireTest, RecoversAfterCircularRequire) {
    ASSERT_FALSE(_runtime.run_string("require('cycle_a')", "probe"));

    EXPECT_TRUE(_runtime.run_string("assert(require('helper').value == 42)", "check"));
}

TEST_F(ScriptRequireTest, RejectsDeclarationInsideModule) {
    EXPECT_FALSE(_runtime.run_string("require('declares')", "probe"));

    EXPECT_TRUE(error_logged({"lib/declares.lua:1:", "entity 'hidden' must be declared"}));
    EXPECT_FALSE(_runtime.find_declaration("hidden"));
}

TEST_F(ScriptRequireTest, RejectsBytecodeModule) {
    EXPECT_FALSE(_runtime.run_string("require('bytecode')", "probe"));

    EXPECT_TRUE(error_logged({"error loading module 'bytecode'"}));
}

TEST(ScriptRequire, FailsBeforeScriptsAreLoaded) {
    auto sink = std::make_shared<logger::MemorySink>();
    const logger::ScopedSink scope{sink, logger::Level::Trace};
    ScriptRuntime runtime;

    EXPECT_FALSE(runtime.run_string("require('helper')", "early"));
    ASSERT_EQ(sink->records().size(), 1U);
    EXPECT_NE(sink->records().front().message.find("only available to loaded scripts"),
              std::string::npos);
}
