#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include "engine/core/ecs/EntityManager.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"
#include "engine/core/math/Transform.hpp"
#include "engine/core/math/Vec2.hpp"
#include "engine/script/ScriptRuntime.hpp"
#include "engine/script/ServerApi.hpp"

namespace logger = engine::core::log;
using engine::core::Entity;
using engine::core::EntityManager;
using engine::core::Rotation;
using engine::core::rotation_of;
using engine::core::scale_of;
using engine::core::Vec2;
using engine::script::ScriptRuntime;
using engine::script::ServerApi;

namespace {

std::filesystem::path transform_fixture() {
    return std::filesystem::path(RTYPE_FIXTURES_DIR) / "scripts" / "transform";
}

class TransformFieldsTest : public ::testing::Test {
protected:
    std::shared_ptr<logger::MemorySink> _sink = std::make_shared<logger::MemorySink>();
    logger::ScopedSink _scope{_sink, logger::Level::Trace};
    EntityManager _world;
    ScriptRuntime _runtime = ScriptRuntime::builder().with<ServerApi>().build();

    void SetUp() override {
        _runtime.attach_world(_world);
        ASSERT_TRUE(_runtime.load_scripts(transform_fixture()));
        ASSERT_TRUE(_runtime.run_string("boss = spawn('boss')", "setup"));
    }

    [[nodiscard]] bool logged_error(const std::string& part) const {
        return std::ranges::any_of(_sink->records(), [&](const auto& record) {
            return record.level == logger::Level::Error &&
                   record.message.find(part) != std::string::npos;
        });
    }

    [[nodiscard]] Entity boss() {
        std::optional<Entity> found;
        _world.query<Rotation>().each([&](Entity entity, Rotation&) { found = entity; });
        EXPECT_TRUE(found.has_value());
        return found.value_or(Entity{0});
    }
};

} // namespace

TEST_F(TransformFieldsTest, DeclarationFillsRotationAndUniformScale) {
    EXPECT_FLOAT_EQ(rotation_of(_world, boss()), 45.F);
    EXPECT_EQ(scale_of(_world, boss()), (Vec2{2.F, 2.F}));
}

TEST_F(TransformFieldsTest, FieldsReadBackFromLua) {
    EXPECT_TRUE(_runtime.run_string(R"(
        assert(boss.rotation == 45)
        assert(boss.scale.x == 2 and boss.scale.y == 2)
        local ship = spawn("ship")
        assert(ship.rotation == nil)
        assert(ship.scale.x == -1 and ship.scale.y == 1)
    )",
                                    "test"));
}

TEST_F(TransformFieldsTest, EntityWithoutTransformReadsNil) {
    EXPECT_TRUE(_runtime.run_string(
        "local rock = spawn('rock') assert(rock.rotation == nil and rock.scale == nil)", "test"));
}

TEST_F(TransformFieldsTest, ScriptsChangeRotationAndScale) {
    ASSERT_TRUE(_runtime.run_string(R"(
        boss.rotation = -90
        boss.scale = { x = 0.5, y = -3 }
    )",
                                    "test"));
    EXPECT_FLOAT_EQ(rotation_of(_world, boss()), -90.F);
    EXPECT_EQ(scale_of(_world, boss()), (Vec2{0.5F, -3.F}));
    ASSERT_TRUE(_runtime.run_string("boss.scale = 4", "test"));
    EXPECT_EQ(scale_of(_world, boss()), (Vec2{4.F, 4.F}));
}

TEST_F(TransformFieldsTest, RejectsNonFiniteRotation) {
    EXPECT_FALSE(_runtime.run_string("boss.rotation = 0/0", "test"));
    EXPECT_TRUE(logged_error("test:1: entity field 'rotation' must be a finite number"));
    EXPECT_FALSE(_runtime.run_string("boss.rotation = math.huge", "test"));
    EXPECT_FLOAT_EQ(rotation_of(_world, boss()), 45.F);
}

TEST_F(TransformFieldsTest, RejectsNonFiniteScale) {
    EXPECT_FALSE(_runtime.run_string("boss.scale = math.huge", "test"));
    EXPECT_TRUE(logged_error("test:1: entity field 'scale' must be a finite number"));
    EXPECT_FALSE(_runtime.run_string("boss.scale = { x = 1, y = -math.huge }", "test"));
    EXPECT_EQ(scale_of(_world, boss()), (Vec2{2.F, 2.F}));
}

TEST_F(TransformFieldsTest, RejectsScaleOfAnotherType) {
    EXPECT_FALSE(_runtime.run_string("boss.scale = 'big'", "test"));
    EXPECT_TRUE(logged_error("entity field 'scale' cannot be set to a string"));
    EXPECT_FALSE(_runtime.run_string("boss.rotation = {}", "test"));
    EXPECT_TRUE(logged_error("entity field 'rotation' cannot be set to a table"));
}

TEST_F(TransformFieldsTest, RejectsScaleTableMissingAnAxis) {
    EXPECT_FALSE(_runtime.run_string("boss.scale = { x = 2 }", "test"));
    EXPECT_TRUE(logged_error("needs a number 'y'"));
    EXPECT_EQ(scale_of(_world, boss()), (Vec2{2.F, 2.F}));
}
