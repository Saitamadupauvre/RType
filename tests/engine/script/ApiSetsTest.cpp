#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "engine/core/EntityManager.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"
#include "engine/script/ClientApi.hpp"
#include "engine/script/EditorApi.hpp"
#include "engine/script/ScriptRuntime.hpp"
#include "engine/script/ServerApi.hpp"

namespace logger = engine::core::log;
using engine::core::EntityManager;
using engine::script::ClientApi;
using engine::script::EditorApi;
using engine::script::ScriptRuntime;
using engine::script::ServerApi;

namespace {

std::filesystem::path api_sets_fixture(const std::string& name) {
    return std::filesystem::path(RTYPE_FIXTURES_DIR) / "scripts" / "api_sets" / name;
}

constexpr const char* core_globals =
    "assert(entity and system and require and sounds and musics and event)";
constexpr const char* server_globals =
    "assert(spawn and players and find_player and play and music and emit)";
constexpr const char* no_server_globals = "assert(not (spawn or players or find_player or play "
                                          "or music or emit))";

class ApiSetsTest : public ::testing::Test {
protected:
    std::shared_ptr<logger::MemorySink> _sink = std::make_shared<logger::MemorySink>();
    logger::ScopedSink _scope{_sink, logger::Level::Trace};

    [[nodiscard]] bool logged_error(const std::string& part) const {
        for (const auto& record : _sink->records()) {
            if (record.level == logger::Level::Error &&
                record.message.find(part) != std::string::npos) {
                return true;
            }
        }
        return false;
    }
};

} // namespace

TEST_F(ApiSetsTest, CoreRuntimeOnlyHasCoreGlobals) {
    auto runtime = ScriptRuntime::builder().build();

    EXPECT_TRUE(runtime.run_string(core_globals, "core"));
    EXPECT_TRUE(runtime.run_string(no_server_globals, "core"));
    EXPECT_TRUE(runtime.run_string("assert(on == nil)", "core"));
}

TEST_F(ApiSetsTest, CoreRuntimeCallingSpawnIsCleanLuaError) {
    auto runtime = ScriptRuntime::builder().build();

    EXPECT_FALSE(runtime.run_string("spawn('ship')", "client.lua"));

    EXPECT_TRUE(logged_error("client.lua:1: attempt to call a nil value (global 'spawn')"));
    EXPECT_TRUE(runtime.run_string("assert(true)", "still_usable"));
}

TEST_F(ApiSetsTest, ServerRuntimeHasServerGlobalsButNotClientOnes) {
    auto runtime = ScriptRuntime::builder().with<ServerApi>().build();

    EXPECT_TRUE(runtime.run_string(core_globals, "server"));
    EXPECT_TRUE(runtime.run_string(server_globals, "server"));
    EXPECT_FALSE(runtime.run_string("on('hit', function() end)", "server.lua"));
    EXPECT_TRUE(logged_error("attempt to call a nil value (global 'on')"));
}

TEST_F(ApiSetsTest, ClientRuntimeHasOnButNotServerGlobals) {
    auto runtime = ScriptRuntime::builder().with<ClientApi>().build();

    EXPECT_TRUE(runtime.run_string(core_globals, "client"));
    EXPECT_TRUE(runtime.run_string("assert(on)", "client"));
    EXPECT_TRUE(runtime.run_string(no_server_globals, "client"));
}

TEST_F(ApiSetsTest, EditorRuntimeOnlyHasCoreGlobals) {
    auto runtime = ScriptRuntime::builder().with<EditorApi>().build();

    EXPECT_TRUE(runtime.run_string(core_globals, "editor"));
    EXPECT_TRUE(runtime.run_string(no_server_globals, "editor"));
}

TEST_F(ApiSetsTest, CoreEntitiesTreatInputAndOwnerAsFreeFields) {
    auto runtime = ScriptRuntime::builder().build();
    EntityManager world;
    runtime.attach_world(world);
    ASSERT_TRUE(runtime.load_scripts(api_sets_fixture("core")));
    const auto probe = runtime.spawn("probe");
    ASSERT_TRUE(probe.has_value());

    EXPECT_TRUE(runtime.call(*probe, "check_free_fields"));
}

TEST_F(ApiSetsTest, BuilderRejectsSetAddedTwice) {
    EXPECT_THROW((void)ScriptRuntime::builder().with<ServerApi>().with<ServerApi>().build(),
                 std::invalid_argument);
}

TEST_F(ApiSetsTest, BuilderRejectsServerWithClient) {
    EXPECT_THROW((void)ScriptRuntime::builder().with<ServerApi>().with<ClientApi>().build(),
                 std::invalid_argument);
}

TEST_F(ApiSetsTest, BuilderAcceptsEditorWithServer) {
    auto runtime = ScriptRuntime::builder().with<ServerApi>().with<EditorApi>().build();

    EXPECT_NO_THROW((void)runtime.api<ServerApi>());
    EXPECT_NO_THROW((void)runtime.api<EditorApi>());
}

TEST_F(ApiSetsTest, BuilderIsEmptyAfterBuild) {
    auto builder = ScriptRuntime::builder();
    builder.with<ServerApi>();
    auto first = builder.build();
    auto second = builder.build();

    EXPECT_NO_THROW((void)first.api<ServerApi>());
    EXPECT_THROW((void)second.api<ServerApi>(), std::out_of_range);
}

TEST_F(ApiSetsTest, ApiThrowsForSetNotAdded) {
    auto runtime = ScriptRuntime::builder().with<ClientApi>().build();

    EXPECT_THROW((void)runtime.api<ServerApi>(), std::out_of_range);
}

TEST_F(ApiSetsTest, ApiReferenceSurvivesMove) {
    auto runtime = ScriptRuntime::builder().with<ServerApi>().build();
    auto& server = runtime.api<ServerApi>();

    ScriptRuntime moved = std::move(runtime);

    EXPECT_EQ(&moved.api<ServerApi>(), &server);
    EXPECT_TRUE(moved.run_string(server_globals, "moved"));
}
