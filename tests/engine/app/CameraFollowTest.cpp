#include <gtest/gtest.h>

#include <string>
#include <utility>

#include "CameraFollow.hpp"
#include "engine/core/Camera.hpp"
#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/Components.hpp"
#include "engine/core/EntityManager.hpp"

using engine::app::follow_cameras;
using engine::core::Camera;
using engine::core::Entity;
using engine::core::EntityManager;
using engine::core::Position;
using engine::core::Tags;
using engine::core::Vec2;

namespace {

class CameraFollowTest : public ::testing::Test {
protected:
    void SetUp() override {
        world.register_component<Position>();
        world.register_component<Tags>();
        world.register_component<Camera>();
    }

    Entity camera(Vec2 position, std::string follow) {
        const Entity entity = world.spawn();
        world.add(entity, Position{position});
        world.add(entity, Camera{.zoom = 1.F,
                                 .follow = std::move(follow),
                                 .bounds_origin = {},
                                 .bounds_size = {}});
        return entity;
    }

    Entity tagged(Vec2 position, std::string tag) {
        const Entity entity = world.spawn();
        world.add(entity, Position{position});
        world.add(entity, Tags{std::move(tag)});
        return entity;
    }

    [[nodiscard]] Vec2 position_of(Entity entity) { return world.get<Position>(entity).pos; }

    EntityManager world;
};

} // namespace

TEST_F(CameraFollowTest, CentersOnTaggedEntity) {
    const Entity view = camera({0.F, 0.F}, "player");
    tagged({300.F, 400.F}, "player");

    follow_cameras(world);

    EXPECT_EQ(position_of(view), (Vec2{300.F, 400.F}));
}

TEST_F(CameraFollowTest, TracksTargetEveryCall) {
    const Entity view = camera({0.F, 0.F}, "player");
    const Entity target = tagged({300.F, 400.F}, "player");
    follow_cameras(world);

    world.get<Position>(target).pos = {350.F, 410.F};
    follow_cameras(world);

    EXPECT_EQ(position_of(view), (Vec2{350.F, 410.F}));
}

TEST_F(CameraFollowTest, StaysWhenNoEntityHasTheTag) {
    const Entity view = camera({12.F, 34.F}, "player");
    tagged({300.F, 400.F}, "enemy");

    follow_cameras(world);

    EXPECT_EQ(position_of(view), (Vec2{12.F, 34.F}));
}

TEST_F(CameraFollowTest, CameraWithoutFollowNeverMoves) {
    const Entity view = camera({12.F, 34.F}, "");
    tagged({300.F, 400.F}, "player");

    follow_cameras(world);

    EXPECT_EQ(position_of(view), (Vec2{12.F, 34.F}));
}

TEST_F(CameraFollowTest, PicksLowestEntityWhenSeveralShareTheTag) {
    const Entity view = camera({0.F, 0.F}, "player");
    tagged({100.F, 100.F}, "player");
    tagged({900.F, 900.F}, "player");

    follow_cameras(world);

    EXPECT_EQ(position_of(view), (Vec2{100.F, 100.F}));
}

TEST_F(CameraFollowTest, IgnoresItsOwnTag) {
    const Entity view = camera({5.F, 5.F}, "player");
    world.add(view, Tags{"player"});

    follow_cameras(world);

    EXPECT_EQ(position_of(view), (Vec2{5.F, 5.F}));
}

TEST(CameraFollow, WorldWithoutCamerasIsLeftUntouched) {
    EntityManager world;
    world.register_component<Position>();
    const Entity entity = world.spawn();
    world.add(entity, Position{1.F, 2.F});

    follow_cameras(world);

    EXPECT_EQ(world.get<Position>(entity).pos, (Vec2{1.F, 2.F}));
}
