#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "engine/core/ecs/ComponentPhysics.hpp"
#include "engine/core/ecs/Entity.hpp"
#include "engine/core/ecs/EntityManager.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"
#include "engine/script/ClientApi.hpp"
#include "engine/script/ScriptRuntime.hpp"

namespace logger = engine::core::log;
using engine::core::Entity;
using engine::core::EntityManager;
using engine::core::Position;
using engine::script::ClientApi;
using engine::script::ScriptRuntime;

namespace {

class LocalEntitiesTest : public ::testing::Test {
protected:
    std::shared_ptr<logger::MemorySink> _sink = std::make_shared<logger::MemorySink>();
    logger::ScopedSink _scope{_sink, logger::Level::Trace};
    EntityManager _world;
    ScriptRuntime _runtime = ScriptRuntime::builder().with<ClientApi>().build();

    void SetUp() override {
        _runtime.attach_world(_world);
        ASSERT_TRUE(_runtime.load_scripts(std::filesystem::path(RTYPE_FIXTURES_DIR) / "scripts" /
                                          "local_entities"));
    }

    [[nodiscard]] bool logged_error(const std::string& part) const {
        return std::ranges::any_of(_sink->records(), [&](const auto& record) {
            return record.level == logger::Level::Error &&
                   record.message.find(part) != std::string::npos;
        });
    }

    [[nodiscard]] std::vector<float> xs() {
        std::vector<float> found;
        _world.query<Position>().each(
            [&](Entity, const Position& position) { found.push_back(position.pos.x); });
        return found;
    }
};

} // namespace

TEST_F(LocalEntitiesTest, SpawnLocalCreatesAnEntityFromItsDeclaration) {
    ASSERT_TRUE(_runtime.run_string(R"(
        spark = spawn_local("spark")
        assert(spark.type == "spark" and spark.sprite == "fx/spark.png")
    )",
                                    "client"));

    EXPECT_EQ(xs(), (std::vector<float>{10.F}));
}

TEST_F(LocalEntitiesTest, OverridesApplyOnSpawn) {
    ASSERT_TRUE(_runtime.run_string("spawn_local('spark', { x = 300, z_index = 4 })", "client"));

    EXPECT_EQ(xs(), (std::vector<float>{300.F}));
}

TEST_F(LocalEntitiesTest, LocalEntitiesRunTheClientSectionOnUpdate) {
    ASSERT_TRUE(_runtime.run_string("spark = spawn_local('spark')", "client"));

    _runtime.update(1.F / 60.F);

    EXPECT_EQ(xs(), (std::vector<float>{11.F}));
    EXPECT_TRUE(_runtime.run_string("assert(spark.lifetime == 1)", "check"));
}

TEST_F(LocalEntitiesTest, DestroyedLocalEntitiesAreRemovedOnFlush) {
    ASSERT_TRUE(
        _runtime.run_string("spawn_local('spark') spawn_local('spark', { x = 0 })", "client"));

    for (int tick = 0; tick < 3; ++tick) {
        _runtime.update(1.F / 60.F);
    }
    EXPECT_EQ(_runtime.flush_destroyed(), 2U);

    EXPECT_TRUE(xs().empty());
}

TEST_F(LocalEntitiesTest, UnknownTypeReturnsNilAndIsLogged) {
    EXPECT_TRUE(_runtime.run_string("assert(spawn_local('sprak') == nil)", "client"));

    EXPECT_TRUE(logged_error("client:1: cannot spawn unknown entity 'sprak'"));
    EXPECT_TRUE(xs().empty());
}

TEST_F(LocalEntitiesTest, BadOverrideRaisesAndSpawnsNothing) {
    EXPECT_FALSE(_runtime.run_string("spawn_local('spark', { x = 'left' })", "client"));

    EXPECT_TRUE(xs().empty());
}

TEST(LocalEntities, SpawnLocalWithoutAWorldReturnsNilAndIsLogged) {
    auto sink = std::make_shared<logger::MemorySink>();
    const logger::ScopedSink scope{sink, logger::Level::Trace};
    ScriptRuntime runtime = ScriptRuntime::builder().with<ClientApi>().build();
    ASSERT_TRUE(runtime.load_scripts(std::filesystem::path(RTYPE_FIXTURES_DIR) / "scripts" /
                                     "local_entities"));

    EXPECT_TRUE(runtime.run_string("assert(spawn_local('spark') == nil)", "client"));

    EXPECT_TRUE(std::ranges::any_of(sink->records(), [](const auto& record) {
        return record.message.find("no world is attached") != std::string::npos;
    }));
}
