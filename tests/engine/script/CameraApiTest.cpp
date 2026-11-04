#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <string>

#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"
#include "engine/core/math/Camera.hpp"
#include "engine/core/math/Vec2.hpp"
#include "engine/script/ClientApi.hpp"
#include "engine/script/ScriptRuntime.hpp"

namespace logger = engine::core::log;
using engine::core::CameraView;
using engine::core::Vec2;
using engine::script::CameraOverride;
using engine::script::ClientApi;
using engine::script::ScriptRuntime;

namespace {

constexpr Vec2 viewport{1920.F, 1080.F};

std::filesystem::path client_fixture() {
    return std::filesystem::path(RTYPE_FIXTURES_DIR) / "scripts" / "api_sets" / "client";
}

class CameraApiTest : public ::testing::Test {
protected:
    std::shared_ptr<logger::MemorySink> _sink = std::make_shared<logger::MemorySink>();
    logger::ScopedSink _scope{_sink, logger::Level::Trace};
    ScriptRuntime _runtime = ScriptRuntime::builder().with<ClientApi>().build();
    ClientApi& _client = _runtime.api<ClientApi>();

    void SetUp() override { ASSERT_TRUE(_runtime.load_scripts(client_fixture())); }

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

TEST_F(CameraApiTest, NoOverrideUntilScriptsAskForOne) {
    EXPECT_EQ(_client.camera_override(), CameraOverride{});
}

TEST_F(CameraApiTest, SetShowsAFixedView) {
    ASSERT_TRUE(_runtime.run_string("camera.set(100, 200, 2)", "client"));

    EXPECT_EQ(_client.camera_override(),
              (CameraOverride{.kind = CameraOverride::Kind::Fixed,
                              .view = {.center = {100.F, 200.F}, .zoom = 2.F},
                              .entity = 0}));
}

TEST_F(CameraApiTest, MissingZoomKeepsTheCurrentOne) {
    _client.set_camera_view({.center = {0.F, 0.F}, .zoom = 1.5F}, viewport);

    ASSERT_TRUE(_runtime.run_string("camera.set(10, 20)", "client"));

    EXPECT_FLOAT_EQ(_client.camera_override().view.zoom, 1.5F);
}

TEST_F(CameraApiTest, FollowTargetsAnEntityId) {
    ASSERT_TRUE(_runtime.run_string("camera.follow(4294967300, 3)", "client"));

    const CameraOverride camera = _client.camera_override();
    EXPECT_EQ(camera.kind, CameraOverride::Kind::Follow);
    EXPECT_EQ(camera.entity, 4294967300U);
    EXPECT_FLOAT_EQ(camera.view.zoom, 3.F);
}

TEST_F(CameraApiTest, ResetDropsTheOverride) {
    ASSERT_TRUE(_runtime.run_string("camera.set(1, 2) camera.reset()", "client"));

    EXPECT_EQ(_client.camera_override(), CameraOverride{});
}

TEST_F(CameraApiTest, OverrideIsLocalToItsClient) {
    ScriptRuntime other_runtime = ScriptRuntime::builder().with<ClientApi>().build();
    ASSERT_TRUE(other_runtime.load_scripts(client_fixture()));

    ASSERT_TRUE(_runtime.run_string("camera.set(500, 500, 4)", "client"));

    EXPECT_EQ(other_runtime.api<ClientApi>().camera_override(), CameraOverride{});
}

TEST_F(CameraApiTest, ScreenToWorldIsIdentityWithDefaultView) {
    EXPECT_TRUE(_runtime.run_string(R"(
        local x, y = camera.screen_to_world(37, 1000)
        assert(x == 37 and y == 1000)
    )",
                                    "client"));
}

TEST_F(CameraApiTest, ScreenToWorldUsesTheViewBeingDrawn) {
    _client.set_camera_view({.center = {100.F, 100.F}, .zoom = 2.F}, viewport);

    EXPECT_TRUE(_runtime.run_string(R"(
        local x, y = camera.screen_to_world(980, 520)
        assert(x == 110 and y == 90)
    )",
                                    "client"));
}

TEST_F(CameraApiTest, RejectsInvalidArgumentsAndKeepsTheOverride) {
    ASSERT_TRUE(_runtime.run_string("camera.set(1, 2, 1)", "client"));
    const CameraOverride before = _client.camera_override();

    EXPECT_FALSE(_runtime.run_string("camera.set('a', 2)", "client"));
    EXPECT_TRUE(logged_error("camera.set: x must be a finite number"));
    EXPECT_FALSE(_runtime.run_string("camera.set(1, 0/0)", "client"));
    EXPECT_TRUE(logged_error("camera.set: y must be a finite number"));
    EXPECT_FALSE(_runtime.run_string("camera.set(1, 2, 0)", "client"));
    EXPECT_TRUE(logged_error("camera.set: zoom must be a positive number"));
    EXPECT_FALSE(_runtime.run_string("camera.set(1, 2, math.huge)", "client"));
    EXPECT_TRUE(logged_error("camera.set: zoom must be a positive number"));
    EXPECT_FALSE(_runtime.run_string("camera.follow(1.5)", "client"));
    EXPECT_FALSE(_runtime.run_string("camera.follow(-1)", "client"));
    EXPECT_FALSE(_runtime.run_string("camera.follow('ship')", "client"));
    EXPECT_TRUE(logged_error("camera.follow: id must be a non-negative integer"));
    EXPECT_FALSE(_runtime.run_string("camera.follow(3, -2)", "client"));
    EXPECT_TRUE(logged_error("camera.follow: zoom must be a positive number"));
    EXPECT_FALSE(_runtime.run_string("camera.screen_to_world(nil, 3)", "client"));
    EXPECT_TRUE(logged_error("camera.screen_to_world: x must be a finite number"));

    EXPECT_EQ(_client.camera_override(), before);
}
