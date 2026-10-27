#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "engine/core/EntityManager.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"
#include "engine/script/ClientApi.hpp"
#include "engine/script/ScriptRuntime.hpp"
#include "engine/script/ServerApi.hpp"

namespace logger = engine::core::log;
using engine::core::EntityManager;
using engine::script::ClientApi;
using engine::script::ScriptRuntime;
using engine::script::ServerApi;

namespace {

std::filesystem::path sections_fixture(const std::string& name) {
    return std::filesystem::path(RTYPE_FIXTURES_DIR) / "scripts" / "sections" / name;
}

class SectionsTest : public ::testing::Test {
protected:
    std::shared_ptr<logger::MemorySink> _sink = std::make_shared<logger::MemorySink>();
    logger::ScopedSink _scope{_sink, logger::Level::Trace};
    EntityManager _world;

    [[nodiscard]] bool logged_error(const std::vector<std::string>& parts) const {
        return std::ranges::any_of(_sink->records(), [&](const logger::MemoryRecord& record) {
            return record.level == logger::Level::Error &&
                   std::ranges::all_of(parts, [&](const std::string& part) {
                       return record.message.find(part) != std::string::npos;
                   });
        });
    }

    bool check(ScriptRuntime& runtime, const std::string& type, const std::string& method) {
        runtime.attach_world(_world);
        const auto entity = runtime.spawn(type);
        return entity && runtime.call(*entity, method);
    }
};

} // namespace

TEST_F(SectionsTest, ServerRuntimeReadsOnlyServerSection) {
    auto runtime = ScriptRuntime::builder().with<ServerApi>().build();
    ASSERT_TRUE(runtime.load_scripts(sections_fixture("split")));

    EXPECT_TRUE(check(runtime, "ship", "check_server"));
    EXPECT_TRUE(check(runtime, "bydo", "check_server"));
}

TEST_F(SectionsTest, ClientRuntimeReadsOnlyClientSection) {
    auto runtime = ScriptRuntime::builder().with<ClientApi>().build();
    ASSERT_TRUE(runtime.load_scripts(sections_fixture("split")));

    EXPECT_TRUE(check(runtime, "ship", "check_client"));
    EXPECT_TRUE(check(runtime, "bydo", "check_client"));
}

TEST_F(SectionsTest, CoreRuntimeReadsNoSection) {
    auto runtime = ScriptRuntime::builder().build();
    ASSERT_TRUE(runtime.load_scripts(sections_fixture("split")));

    EXPECT_TRUE(check(runtime, "ship", "check_core"));
}

TEST_F(SectionsTest, CallbackInServerSectionRunsOnlyOnServer) {
    auto server = ScriptRuntime::builder().with<ServerApi>().build();
    auto client = ScriptRuntime::builder().with<ClientApi>().build();
    ASSERT_TRUE(server.load_scripts(sections_fixture("split")));
    ASSERT_TRUE(client.load_scripts(sections_fixture("split")));
    EntityManager client_world;
    server.attach_world(_world);
    client.attach_world(client_world);
    const auto on_server = server.spawn("ship");
    const auto on_client = client.spawn("ship");
    ASSERT_TRUE(on_server && on_client);

    server.update(0.1F);
    server.update(0.1F);
    client.update(0.1F);
    client.update(0.1F);

    EXPECT_TRUE(server.call(*on_server, "check_two_ticks"));
    EXPECT_TRUE(client.call(*on_client, "check_no_ticks"));
}

TEST_F(SectionsTest, RejectsFileSettingNameInsideAndOutsideSection) {
    auto runtime = ScriptRuntime::builder().with<ServerApi>().build();

    EXPECT_FALSE(runtime.load_scripts(sections_fixture("conflict")));

    EXPECT_FALSE(runtime.find_declaration("ship").has_value());
    EXPECT_FALSE(runtime.find_declaration("ship_shot").has_value());
    EXPECT_TRUE(runtime.find_declaration("other").has_value());
    EXPECT_TRUE(logged_error({"ship.lua", "entity 'ship'", "'speed'", "section 'server'"}));
}

TEST_F(SectionsTest, RejectsConflictInOtherSideSectionToo) {
    auto runtime = ScriptRuntime::builder().with<ServerApi>().build();

    EXPECT_FALSE(runtime.load_scripts(sections_fixture("client_conflict")));

    EXPECT_FALSE(runtime.find_declaration("ship").has_value());
    EXPECT_TRUE(logged_error({"entity 'ship'", "'speed'", "section 'client'"}));
}

TEST_F(SectionsTest, RejectsSectionThatIsNotATable) {
    auto runtime = ScriptRuntime::builder().with<ClientApi>().build();

    EXPECT_FALSE(runtime.load_scripts(sections_fixture("not_table")));

    EXPECT_FALSE(runtime.find_declaration("ship").has_value());
    EXPECT_TRUE(logged_error({"entity 'ship'", "section 'server' must be a table"}));
}

TEST_F(SectionsTest, WarnsWhenSectionSetsReservedSectionName) {
    auto runtime = ScriptRuntime::builder().with<ClientApi>().build();

    EXPECT_TRUE(runtime.load_scripts(sections_fixture("nested")));

    EXPECT_TRUE(std::ranges::any_of(_sink->records(), [](const logger::MemoryRecord& record) {
        return record.level == logger::Level::Warn &&
               record.message.find("entity 'ship' defines reserved name 'server'") !=
                   std::string::npos;
    }));
}
