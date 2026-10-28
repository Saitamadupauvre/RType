#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/Vec2.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"
#include "engine/physics/Components.hpp"
#include "engine/script/ScriptRuntime.hpp"
#include "engine/script/ServerApi.hpp"

namespace logger = engine::core::log;
using engine::core::Entity;
using engine::core::EntityManager;
using engine::core::Position;
using engine::core::Vec2;
using engine::physics::AABBCollider;
using engine::physics::PhysicsBody;
using engine::physics::Scriptable;
using engine::script::ScriptRuntime;
using engine::script::ServerApi;

namespace {

std::filesystem::path collisions_fixture() {
    return std::filesystem::path(RTYPE_FIXTURES_DIR) / "scripts" / "collisions";
}

class ScriptCollisionsTest : public ::testing::Test {
protected:
    std::shared_ptr<logger::MemorySink> _sink = std::make_shared<logger::MemorySink>();
    logger::ScopedSink _scope{_sink, logger::Level::Trace};
    EntityManager _world;
    ScriptRuntime _runtime = ScriptRuntime::builder().with<ServerApi>().build();
    ServerApi& _server = _runtime.api<ServerApi>();

    void SetUp() override {
        _runtime.attach_world(_world);
        ASSERT_TRUE(_runtime.load_scripts(collisions_fixture()));
    }

    Entity spawn(const std::string& type) {
        const std::optional<Entity> entity = _runtime.spawn(type, Vec2{0.F, 0.F});
        if (!entity) {
            throw std::runtime_error("cannot spawn " + type);
        }
        return *entity;
    }

    [[nodiscard]] float hits(Entity entity) { return _world.get<Position>(entity).pos.x; }

    [[nodiscard]] bool logged_error(const std::string& part) const {
        return std::ranges::any_of(_sink->records(), [&](const logger::MemoryRecord& record) {
            return record.level == logger::Level::Error &&
                   record.message.find(part) != std::string::npos;
        });
    }
};

} // namespace

TEST_F(ScriptCollisionsTest, HitboxGivesATopLeftAnchoredColliderThatOnlyReports) {
    const Entity hunter = spawn("hunter");

    const AABBCollider& collider = _world.get<AABBCollider>(hunter);
    EXPECT_EQ(collider.size, (Vec2{10.F, 20.F}));
    EXPECT_EQ(collider.offset, (Vec2{5.F, 10.F}));
    const PhysicsBody& body = _world.get<PhysicsBody>(hunter);
    EXPECT_EQ(body.type, PhysicsBody::Type::Dynamic);
    EXPECT_FALSE(body.resolve_collisions);
    EXPECT_FALSE(body.affected_by_gravity);
    EXPECT_TRUE(_world.has<Scriptable>(hunter));
}

TEST_F(ScriptCollisionsTest, ScriptsReadHitboxAndFilterBack) {
    const Entity hunter = spawn("hunter");

    ASSERT_TRUE(_runtime.call(hunter, "measure"));
    EXPECT_EQ(_world.get<Position>(hunter).pos, (Vec2{10.F, 20.F}));
    ASSERT_TRUE(_runtime.call(hunter, "count_filters"));
    EXPECT_EQ(hits(hunter), 1.F);
}

TEST_F(ScriptCollisionsTest, OnlyTheEntityCollidingWithTheOthersTagIsCalled) {
    const Entity hunter = spawn("hunter");
    const Entity prey = spawn("prey");

    _runtime.collide(prey, hunter);

    EXPECT_EQ(hits(hunter), 1.F);
    EXPECT_EQ(_world.get<Position>(hunter).pos.y, static_cast<float>(prey.id()));
    EXPECT_EQ(hits(prey), 0.F);
}

TEST_F(ScriptCollisionsTest, BothEntitiesAreCalledWhenEachCollidesWithTheOther) {
    const Entity first = spawn("duelist");
    const Entity second = spawn("duelist");

    _runtime.collide(first, second);

    EXPECT_EQ(hits(first), 1.F);
    EXPECT_EQ(hits(second), 1.F);
}

TEST_F(ScriptCollisionsTest, SecondCallbackStillRunsWhenTheFirstDestroysIt) {
    const Entity killer = spawn("killer");
    const Entity hunter = spawn("hunter");

    _runtime.collide(killer, hunter);

    EXPECT_EQ(hits(killer), 1.F);
    EXPECT_EQ(hits(hunter), 1.F);
    EXPECT_EQ(_runtime.flush_destroyed(), 1U);
    EXPECT_FALSE(_world.alive(hunter));
}

TEST_F(ScriptCollisionsTest, EntityDestroyedEarlierInTheTickGetsNoMoreCollisions) {
    const Entity killer = spawn("killer");
    const Entity hunter = spawn("hunter");
    const Entity prey = spawn("prey");

    _runtime.collide(killer, hunter);
    _runtime.collide(hunter, prey);

    EXPECT_EQ(hits(hunter), 1.F);
    EXPECT_EQ(hits(prey), 0.F);
}

TEST_F(ScriptCollisionsTest, UnknownOrDeadEntitiesAreIgnored) {
    const Entity hunter = spawn("hunter");
    const Entity prey = spawn("prey");
    const Entity stranger = _world.spawn();
    _runtime.release(prey);
    _world.kill(prey);

    _runtime.collide(hunter, prey);
    _runtime.collide(stranger, hunter);

    EXPECT_EQ(hits(hunter), 0.F);
    EXPECT_FALSE(logged_error("on_collide"));
}

TEST_F(ScriptCollisionsTest, CallbackErrorIsLoggedAndOtherCallbacksStillRun) {
    const Entity broken = spawn("broken");
    const Entity hunter = spawn("hunter");

    _runtime.collide(broken, hunter);

    EXPECT_TRUE(logged_error("collision exploded"));
    EXPECT_EQ(hits(hunter), 1.F);
}

TEST_F(ScriptCollisionsTest, ChangingTagsOrFilterAtRuntimeChangesWhoCollides) {
    const Entity hunter = spawn("hunter");
    const Entity prey = spawn("prey");

    ASSERT_TRUE(_runtime.call(prey, "become_duelist"));
    _runtime.collide(hunter, prey);
    EXPECT_EQ(hits(hunter), 0.F);

    ASSERT_TRUE(_runtime.call(hunter, "hunt_duelists"));
    _runtime.collide(hunter, prey);
    EXPECT_EQ(hits(hunter), 1.F);
}

TEST_F(ScriptCollisionsTest, InvalidHitboxOrFilterFailsTheSpawn) {
    EXPECT_FALSE(_runtime.spawn("flat", std::nullopt));
    EXPECT_TRUE(logged_error("entity field 'hitbox': size must be positive"));
    EXPECT_FALSE(_runtime.spawn("shapeless", std::nullopt));
    EXPECT_TRUE(logged_error("entity field 'hitbox' cannot be set to a string"));
    EXPECT_FALSE(_runtime.spawn("half", std::nullopt));
    EXPECT_TRUE(logged_error("needs a number 'h'"));
    EXPECT_FALSE(_runtime.spawn("picky", std::nullopt));
    EXPECT_TRUE(logged_error("entity field 'collides_with' must only hold strings"));
}
