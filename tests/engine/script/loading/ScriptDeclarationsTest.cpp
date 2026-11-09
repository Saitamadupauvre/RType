#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"
#include "engine/script/ScriptRuntime.hpp"
#include "engine/script/ServerApi.hpp"

namespace logger = engine::core::log;
using engine::script::DeclarationInfo;
using engine::script::DeclarationKind;
using engine::script::ScriptRuntime;
using engine::script::ServerApi;

namespace {

std::filesystem::path declarations_fixture(const std::string& name) {
    return std::filesystem::path(TEST_FIXTURES_DIR) / "scripts" / "declarations" / name;
}

class ScriptDeclarationsTest : public ::testing::Test {
protected:
    std::shared_ptr<logger::MemorySink> _sink = std::make_shared<logger::MemorySink>();
    logger::ScopedSink _scope{_sink, logger::Level::Trace};
    ScriptRuntime _runtime = ScriptRuntime::builder().with<ServerApi>().build();
    ServerApi& _server = _runtime.api<ServerApi>();

    [[nodiscard]] std::vector<std::string> messages(logger::Level level) const {
        std::vector<std::string> result;
        for (const auto& record : _sink->records()) {
            if (record.level == level) {
                result.push_back(record.message);
            }
        }
        return result;
    }

    [[nodiscard]] bool logged(logger::Level level, const std::vector<std::string>& parts) const {
        return std::ranges::any_of(messages(level), [&](const std::string& message) {
            return std::ranges::all_of(parts, [&](const std::string& part) {
                return message.find(part) != std::string::npos;
            });
        });
    }
};

} // namespace

TEST_F(ScriptDeclarationsTest, RegistersShortFormEntity) {
    EXPECT_TRUE(_runtime.load_scripts(declarations_fixture("short_form")));

    EXPECT_EQ(_runtime.find_declaration("enemy"),
              (DeclarationInfo{DeclarationKind::Entity, "stage1/enemy.lua"}));
    EXPECT_TRUE(messages(logger::Level::Error).empty());
}

TEST_F(ScriptDeclarationsTest, RegistersSeveralTableFormsInOneFile) {
    EXPECT_TRUE(_runtime.load_scripts(declarations_fixture("table_form")));

    EXPECT_EQ(_runtime.find_declaration("player"),
              (DeclarationInfo{DeclarationKind::Entity, "player/player.lua"}));
    EXPECT_EQ(_runtime.find_declaration("player_missile"),
              (DeclarationInfo{DeclarationKind::Entity, "player/player.lua"}));
    EXPECT_EQ(_runtime.find_declaration("respawn"),
              (DeclarationInfo{DeclarationKind::System, "player/player.lua"}));
}

TEST_F(ScriptDeclarationsTest, CallsOnStartFromMainAfterLoading) {
    ASSERT_TRUE(_runtime.load_scripts(declarations_fixture("short_form")));

    EXPECT_TRUE(_runtime.run_string("assert(started == true)", "check"));
}

TEST_F(ScriptDeclarationsTest, ShortFormFileGlobalsStayInItsEnvironment) {
    ASSERT_TRUE(_runtime.load_scripts(declarations_fixture("short_form")));

    EXPECT_TRUE(_runtime.run_string("assert(sprite == nil and update == nil)", "check"));
}

TEST_F(ScriptDeclarationsTest, RejectsTwoShortFormsInOneFile) {
    EXPECT_FALSE(_runtime.load_scripts(declarations_fixture("two_short")));

    EXPECT_FALSE(_runtime.find_declaration("first"));
    EXPECT_FALSE(_runtime.find_declaration("second"));
    EXPECT_TRUE(
        logged(logger::Level::Error, {"bad.lua", "only one short-form", "'first'", "'second'"}));
}

TEST_F(ScriptDeclarationsTest, KeepsLoadingOtherFilesAfterRejectedFile) {
    EXPECT_FALSE(_runtime.load_scripts(declarations_fixture("two_short")));

    EXPECT_TRUE(_runtime.find_declaration("good"));
}

TEST_F(ScriptDeclarationsTest, RejectsShortFormMixedWithTableForm) {
    EXPECT_FALSE(_runtime.load_scripts(declarations_fixture("mixed")));

    EXPECT_FALSE(_runtime.find_declaration("short"));
    EXPECT_FALSE(_runtime.find_declaration("table"));
    EXPECT_TRUE(logged(logger::Level::Error, {"bad.lua", "'short'", "cannot be mixed"}));
}

TEST_F(ScriptDeclarationsTest, RejectsDuplicateNameAcrossFilesShowingBothPaths) {
    EXPECT_FALSE(_runtime.load_scripts(declarations_fixture("duplicate")));

    EXPECT_EQ(_runtime.find_declaration("enemy"),
              (DeclarationInfo{DeclarationKind::Entity, "a/first.lua"}));
    EXPECT_TRUE(logged(logger::Level::Error,
                       {"duplicate declaration 'enemy'", "b/second.lua", "a/first.lua"}));
}

TEST_F(ScriptDeclarationsTest, RejectedDuplicateFileRegistersNothing) {
    EXPECT_FALSE(_runtime.load_scripts(declarations_fixture("duplicate")));

    EXPECT_FALSE(_runtime.find_declaration("other"));
}

TEST_F(ScriptDeclarationsTest, RejectsDuplicateNameInOneFile) {
    EXPECT_FALSE(_runtime.load_scripts(declarations_fixture("duplicate_in_file")));

    EXPECT_FALSE(_runtime.find_declaration("twin"));
    EXPECT_TRUE(logged(logger::Level::Error, {"duplicate declaration 'twin'", "twice.lua"}));
}

TEST_F(ScriptDeclarationsTest, WarnsOnReservedDeclarationName) {
    EXPECT_TRUE(_runtime.load_scripts(declarations_fixture("reserved")));

    EXPECT_TRUE(_runtime.find_declaration("spawn"));
    EXPECT_TRUE(logged(logger::Level::Warn, {"names.lua", "entity name 'spawn' is reserved"}));
}

TEST_F(ScriptDeclarationsTest, WarnsOnReservedGlobalAndEngineOwnedFields) {
    EXPECT_TRUE(_runtime.load_scripts(declarations_fixture("reserved")));

    EXPECT_TRUE(logged(logger::Level::Warn, {"'ship'", "reserved name 'play'"}));
    EXPECT_TRUE(logged(logger::Level::Warn, {"'ship'", "reserved name 'id'"}));
}

TEST_F(ScriptDeclarationsTest, WarnsOnCallbackThatIsNotAFunction) {
    EXPECT_TRUE(_runtime.load_scripts(declarations_fixture("reserved")));

    EXPECT_TRUE(logged(logger::Level::Warn, {"'ship'", "callback 'update' must be a function"}));
}

TEST_F(ScriptDeclarationsTest, DoesNotWarnOnNativeFieldsOrFunctionCallbacks) {
    EXPECT_TRUE(_runtime.load_scripts(declarations_fixture("reserved")));

    EXPECT_FALSE(logged(logger::Level::Warn, {"'sprite'"}));
    EXPECT_FALSE(logged(logger::Level::Warn, {"'on_collide'"}));
}

TEST_F(ScriptDeclarationsTest, WarnsOnGlobalsOutsideATableForm) {
    EXPECT_TRUE(_runtime.load_scripts(declarations_fixture("lint")));

    EXPECT_TRUE(logged(logger::Level::Warn,
                       {"player.lua", "'update' is defined outside the table of 'player'"}));
    EXPECT_TRUE(logged(logger::Level::Warn, {"player.lua", "'speed_bonus'"}));
    EXPECT_FALSE(logged(logger::Level::Warn, {"ship.lua", "outside"}));
    EXPECT_FALSE(logged(logger::Level::Warn, {"main.lua", "outside"}));
}

TEST_F(ScriptDeclarationsTest, SuggestsTheCallbackAMisspelledFunctionMeant) {
    EXPECT_TRUE(_runtime.load_scripts(declarations_fixture("lint")));

    EXPECT_TRUE(logged(logger::Level::Warn, {"ship.lua", "'on_collision' is never called",
                                             "did you mean 'on_collide'"}));
    EXPECT_TRUE(logged(logger::Level::Warn, {"'updte' is never called", "'update'"}));
}

TEST_F(ScriptDeclarationsTest, DoesNotWarnOnCustomMethods) {
    EXPECT_TRUE(_runtime.load_scripts(declarations_fixture("lint")));

    EXPECT_FALSE(logged(logger::Level::Warn, {"'on_hit'"}));
    EXPECT_FALSE(logged(logger::Level::Warn, {"'fire'"}));
    EXPECT_FALSE(logged(logger::Level::Warn, {"'on_collide' is never called"}));
}

TEST_F(ScriptDeclarationsTest, ExplainsCallsToPlannedFunctions) {
    ASSERT_TRUE(_runtime.load_scripts(declarations_fixture("short_form")));

    EXPECT_FALSE(_runtime.run_string("after(2, function() end)", "check"));

    EXPECT_TRUE(logged(logger::Level::Error,
                       {"global 'after') ('after' is planned but not implemented yet"}));
}

TEST_F(ScriptDeclarationsTest, LeavesOtherNilErrorsAlone) {
    ASSERT_TRUE(_runtime.load_scripts(declarations_fixture("short_form")));

    EXPECT_FALSE(_runtime.run_string("undefined_thing()", "check"));

    EXPECT_TRUE(logged(logger::Level::Error, {"global 'undefined_thing'"}));
    EXPECT_FALSE(logged(logger::Level::Error, {"not implemented"}));
}

TEST_F(ScriptDeclarationsTest, WarnsOnFileDeclaringNothing) {
    EXPECT_TRUE(_runtime.load_scripts(declarations_fixture("empty_file")));

    EXPECT_TRUE(logged(logger::Level::Warn, {"helper.lua", "declares nothing"}));
    EXPECT_FALSE(logged(logger::Level::Warn, {"main.lua", "declares nothing"}));
}

TEST_F(ScriptDeclarationsTest, FileWithRuntimeErrorRegistersNothing) {
    EXPECT_FALSE(_runtime.load_scripts(declarations_fixture("broken_file")));

    EXPECT_FALSE(_runtime.find_declaration("lost"));
    EXPECT_TRUE(_runtime.find_declaration("fine"));
    EXPECT_TRUE(logged(logger::Level::Error, {"broken.lua:2:", "boom"}));
}

TEST_F(ScriptDeclarationsTest, RejectsSecondBody) {
    EXPECT_FALSE(_runtime.load_scripts(declarations_fixture("double_body")));

    EXPECT_FALSE(_runtime.find_declaration("twice"));
    EXPECT_TRUE(logged(logger::Level::Error, {"twice.lua:3:", "'twice' already has a body"}));
}

TEST_F(ScriptDeclarationsTest, RejectsBodyGivenAfterFileFinishedLoading) {
    EXPECT_FALSE(_runtime.load_scripts(declarations_fixture("late_body")));

    EXPECT_TRUE(logged(logger::Level::Error,
                       {"main.lua:2:", "'late' can only receive its body while holder.lua"}));
}

TEST_F(ScriptDeclarationsTest, RejectsDeclarationOutsideScriptFile) {
    EXPECT_FALSE(_runtime.run_string("entity 'loose' {}", "inline"));

    EXPECT_TRUE(logged(logger::Level::Error, {"inline:1:", "entity 'loose' must be declared"}));
    EXPECT_FALSE(_runtime.find_declaration("loose"));
}

TEST_F(ScriptDeclarationsTest, FailsWithoutMain) {
    EXPECT_FALSE(_runtime.load_scripts(declarations_fixture("no_main")));

    EXPECT_TRUE(logged(logger::Level::Error, {"has no main.lua"}));
}

TEST_F(ScriptDeclarationsTest, WarnsWhenMainHasNoOnStart) {
    EXPECT_TRUE(_runtime.load_scripts(declarations_fixture("no_on_start")));

    EXPECT_TRUE(logged(logger::Level::Warn, {"does not define on_start()"}));
}

TEST_F(ScriptDeclarationsTest, ReportsErrorRaisedByOnStart) {
    EXPECT_FALSE(_runtime.load_scripts(declarations_fixture("failing_start")));

    EXPECT_TRUE(logged(logger::Level::Error, {"main.lua:2:", "start failed"}));
}

TEST_F(ScriptDeclarationsTest, FailsOnMissingFolder) {
    EXPECT_FALSE(_runtime.load_scripts(declarations_fixture("does_not_exist")));

    EXPECT_TRUE(logged(logger::Level::Error, {"does_not_exist", "does not exist"}));
}

TEST_F(ScriptDeclarationsTest, RefusesToLoadTwice) {
    ASSERT_TRUE(_runtime.load_scripts(declarations_fixture("table_form")));

    EXPECT_FALSE(_runtime.load_scripts(declarations_fixture("short_form")));
    EXPECT_FALSE(_runtime.find_declaration("enemy"));
    EXPECT_TRUE(logged(logger::Level::Error, {"already loaded"}));
}

TEST_F(ScriptDeclarationsTest, UnknownNameIsNotFound) {
    ASSERT_TRUE(_runtime.load_scripts(declarations_fixture("table_form")));

    EXPECT_FALSE(_runtime.find_declaration("ghost"));
}
