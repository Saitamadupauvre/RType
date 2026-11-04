#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include "engine/core/ecs/ComponentPhysics.hpp"
#include "engine/core/ecs/EntityManager.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"
#include "engine/core/math/Camera.hpp"
#include "engine/core/math/Vec2.hpp"
#include "engine/script/ScriptRuntime.hpp"
#include "engine/script/ServerApi.hpp"

namespace logger = engine::core::log;
using engine::core::Camera;
using engine::core::Entity;
using engine::core::EntityManager;
using engine::core::Position;
using engine::core::Vec2;
using engine::script::ScriptRuntime;
using engine::script::ServerApi;

namespace {

std::filesystem::path camera_fixture() {
    return std::filesystem::path(RTYPE_FIXTURES_DIR) / "scripts" / "camera";
}

class CameraFieldsTest : public ::testing::Test {
protected:
    std::shared_ptr<logger::MemorySink> _sink = std::make_shared<logger::MemorySink>();
    logger::ScopedSink _scope{_sink, logger::Level::Trace};
    EntityManager _world;
    ScriptRuntime _runtime = ScriptRuntime::builder().with<ServerApi>().build();

    void SetUp() override {
        _runtime.attach_world(_world);
        ASSERT_TRUE(_runtime.load_scripts(camera_fixture()));
        ASSERT_TRUE(_runtime.run_string("view = spawn('view') ship = spawn('ship')", "setup"));
    }

    [[nodiscard]] bool logged_error(const std::string& part) const {
        return std::ranges::any_of(_sink->records(), [&](const auto& record) {
            return record.level == logger::Level::Error &&
                   record.message.find(part) != std::string::npos;
        });
    }

    [[nodiscard]] Entity camera() {
        std::optional<Entity> found;
        _world.query<Camera>().each([&](Entity entity, Camera&) { found = entity; });
        EXPECT_TRUE(found.has_value());
        return found.value_or(Entity{0});
    }
};

} // namespace

TEST_F(CameraFieldsTest, DeclarationFillsTheCameraComponent) {
    const Camera& view = _world.get<Camera>(camera());
    EXPECT_FLOAT_EQ(view.zoom, 2.F);
    EXPECT_EQ(view.follow, "player");
    EXPECT_EQ(view.bounds_origin, (Vec2{0.F, 0.F}));
    EXPECT_EQ(view.bounds_size, (Vec2{4000.F, 1080.F}));
    EXPECT_EQ(_world.get<Position>(camera()).pos, (Vec2{960.F, 540.F}));
}

TEST_F(CameraFieldsTest, FieldsReadBackFromLua) {
    EXPECT_TRUE(_runtime.run_string(R"(
        assert(view.zoom == 2 and view.follow == "player")
        assert(view.bounds.x == 0 and view.bounds.y == 0)
        assert(view.bounds.w == 4000 and view.bounds.h == 1080)
    )",
                                    "test"));
}

TEST_F(CameraFieldsTest, EntityWithoutCameraReadsNil) {
    EXPECT_TRUE(_runtime.run_string(
        "assert(ship.zoom == nil and ship.follow == nil and ship.bounds == nil)", "test"));
}

TEST_F(CameraFieldsTest, ScriptsChangeTheCamera) {
    ASSERT_TRUE(_runtime.run_string(R"(
        view.zoom = 0.5
        view.follow = ""
        view.bounds = { x = -10, y = 20, w = 0, h = 0 }
    )",
                                    "test"));
    const Camera& view = _world.get<Camera>(camera());
    EXPECT_FLOAT_EQ(view.zoom, 0.5F);
    EXPECT_TRUE(view.follow.empty());
    EXPECT_EQ(view.bounds_origin, (Vec2{-10.F, 20.F}));
    EXPECT_FALSE(view.bounded());
}

TEST_F(CameraFieldsTest, RejectsZoomThatIsNotPositive) {
    EXPECT_FALSE(_runtime.run_string("view.zoom = 0", "test"));
    EXPECT_TRUE(logged_error("test:1: entity field 'zoom': zoom must be positive"));
    EXPECT_FLOAT_EQ(_world.get<Camera>(camera()).zoom, 2.F);
}

TEST_F(CameraFieldsTest, RejectsNegativeBoundsAndKeepsTheOldOnes) {
    EXPECT_FALSE(_runtime.run_string("view.bounds = { x = 5, y = 5, w = -1, h = 10 }", "test"));
    EXPECT_TRUE(logged_error("entity field 'bounds': bounds size must be zero or positive"));
    const Camera& view = _world.get<Camera>(camera());
    EXPECT_EQ(view.bounds_origin, (Vec2{0.F, 0.F}));
    EXPECT_EQ(view.bounds_size, (Vec2{4000.F, 1080.F}));
}

TEST_F(CameraFieldsTest, RejectsBoundsMissingAKey) {
    EXPECT_FALSE(_runtime.run_string("view.bounds = { x = 5, y = 5, w = 10 }", "test"));
    EXPECT_TRUE(logged_error("needs a number 'h'"));
}

TEST_F(CameraFieldsTest, ZoomOnAnEntityAddsACamera) {
    ASSERT_TRUE(_runtime.run_string("ship.zoom = 3", "test"));
    EXPECT_TRUE(_runtime.run_string("assert(ship.zoom == 3 and ship.follow == '')", "test"));
}
