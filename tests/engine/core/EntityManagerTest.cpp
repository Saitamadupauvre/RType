#include <gtest/gtest.h>

#include <set>
#include <stdexcept>
#include <string>

#include "engine/core/EntityManager.hpp"

using engine::core::ComponentAlreadyRegistered;
using engine::core::ComponentNotRegistered;
using engine::core::DistinctTypes;
using engine::core::Entity;
using engine::core::EntityManager;

namespace {

struct Position {
    int x;
    int y;
};

struct Velocity {
    int dx;
    int dy;
};

EntityManager make_manager() {
    EntityManager manager;
    manager.register_component<Position>();
    manager.register_component<Velocity>();
    return manager;
}

} // namespace

static_assert(DistinctTypes<Position, Velocity>);
static_assert(!DistinctTypes<Position, Position>);
static_assert(!DistinctTypes<Position, Velocity, Position>);

TEST(EntityManager, SpawnsDistinctLivingEntities) {
    EntityManager manager;

    const Entity first = manager.spawn();
    const Entity second = manager.spawn();

    EXPECT_NE(first, second);
    EXPECT_TRUE(manager.alive(first));
    EXPECT_TRUE(manager.alive(second));
}

TEST(EntityManager, KillRemovesAllComponents) {
    EntityManager manager = make_manager();
    const Entity entity = manager.spawn();
    manager.add(entity, Position{.x = 1, .y = 2});
    manager.add(entity, Velocity{.dx = 3, .dy = 4});

    EXPECT_TRUE(manager.kill(entity));

    EXPECT_FALSE(manager.alive(entity));
    EXPECT_THROW(static_cast<void>(manager.get<Position>(entity)), std::out_of_range);
    EXPECT_THROW(static_cast<void>(manager.get<Velocity>(entity)), std::out_of_range);
}

TEST(EntityManager, KillIgnoresStaleHandle) {
    EntityManager manager = make_manager();
    const Entity stale = manager.spawn();
    manager.kill(stale);
    const Entity fresh = manager.spawn();
    manager.add(fresh, Position{.x = 5, .y = 6});

    EXPECT_FALSE(manager.kill(stale));

    EXPECT_TRUE(manager.alive(fresh));
    EXPECT_EQ(manager.get<Position>(fresh).x, 5);
}

TEST(EntityManager, RecycledSlotHasNoOldComponents) {
    EntityManager manager = make_manager();
    const Entity old_entity = manager.spawn();
    manager.add(old_entity, Position{.x = 1, .y = 1});
    manager.kill(old_entity);

    const Entity recycled = manager.spawn();

    EXPECT_EQ(recycled.index(), old_entity.index());
    EXPECT_THROW(static_cast<void>(manager.get<Position>(recycled)), std::out_of_range);
}

TEST(EntityManager, AddReplacesExistingComponent) {
    EntityManager manager = make_manager();
    const Entity entity = manager.spawn();
    manager.add(entity, Position{.x = 1, .y = 2});

    manager.add(entity, Position{.x = 7, .y = 8});

    EXPECT_EQ(manager.get<Position>(entity).x, 7);
    EXPECT_EQ(manager.get<Position>(entity).y, 8);
}

TEST(EntityManager, RemoveReportsWhetherAComponentWasRemoved) {
    EntityManager manager = make_manager();
    const Entity entity = manager.spawn();
    manager.add(entity, Position{.x = 1, .y = 2});

    EXPECT_TRUE(manager.remove<Position>(entity));
    EXPECT_FALSE(manager.remove<Position>(entity));
    EXPECT_FALSE(manager.remove<Velocity>(entity));
}

TEST(EntityManager, AddThrowsOnDeadEntity) {
    EntityManager manager = make_manager();
    const Entity entity = manager.spawn();
    manager.kill(entity);

    EXPECT_THROW(manager.add(entity, Position{.x = 0, .y = 0}), std::invalid_argument);
}

TEST(EntityManager, AddThrowsWhenComponentNotRegistered) {
    EntityManager manager;
    const Entity entity = manager.spawn();

    EXPECT_THROW(manager.add(entity, Position{.x = 0, .y = 0}), ComponentNotRegistered);
}

TEST(EntityManager, UnregisteredErrorNamesTheType) {
    EntityManager manager;
    const Entity entity = manager.spawn();

    try {
        static_cast<void>(manager.get<Position>(entity));
        FAIL() << "expected ComponentNotRegistered";
    } catch (const ComponentNotRegistered& error) {
        EXPECT_NE(std::string(error.what()).find("Position"), std::string::npos);
    }
}

TEST(EntityManager, RemoveAndGetThrowWhenComponentNotRegistered) {
    EntityManager manager;
    const Entity entity = manager.spawn();

    EXPECT_THROW(manager.remove<Position>(entity), ComponentNotRegistered);
    EXPECT_THROW(static_cast<void>(manager.get<Position>(entity)), ComponentNotRegistered);
}

TEST(EntityManager, QueryThrowsWhenComponentNotRegistered) {
    EntityManager manager;
    manager.register_component<Position>();

    EXPECT_THROW(static_cast<void>(manager.query<Position, Velocity>()), ComponentNotRegistered);
}

TEST(EntityManager, RegisterTwiceThrows) {
    EntityManager manager;
    manager.register_component<Position>();

    EXPECT_THROW(manager.register_component<Position>(), ComponentAlreadyRegistered);
}

TEST(EntityManager, QuerySkipsEntitiesMissingAComponent) {
    EntityManager manager = make_manager();
    const Entity both = manager.spawn();
    const Entity only_position = manager.spawn();
    const Entity only_velocity = manager.spawn();
    manager.add(both, Position{.x = 1, .y = 1});
    manager.add(both, Velocity{.dx = 2, .dy = 2});
    manager.add(only_position, Position{.x = 3, .y = 3});
    manager.add(only_velocity, Velocity{.dx = 4, .dy = 4});

    std::set<Entity::Id> visited;
    manager.query<Position, Velocity>().each(
        [&](Entity entity, Position&, Velocity&) { visited.insert(entity.id()); });

    EXPECT_EQ(visited, std::set<Entity::Id>{both.id()});
}

TEST(EntityManager, QueryGivesSameResultWhicheverPoolIsSmaller) {
    EntityManager manager = make_manager();
    const Entity shared = manager.spawn();
    manager.add(shared, Position{.x = 1, .y = 1});
    manager.add(shared, Velocity{.dx = 2, .dy = 2});
    for (int i = 0; i < 3; ++i) {
        manager.add(manager.spawn(), Position{.x = i, .y = i});
    }

    int forward = 0;
    manager.query<Position, Velocity>().each([&](Entity, Position&, Velocity&) { ++forward; });
    int backward = 0;
    manager.query<Velocity, Position>().each([&](Entity, Velocity&, Position&) { ++backward; });

    EXPECT_EQ(forward, 1);
    EXPECT_EQ(backward, 1);
}

TEST(EntityManager, QueryDoesNotVisitKilledEntities) {
    EntityManager manager = make_manager();
    const Entity alive = manager.spawn();
    const Entity killed = manager.spawn();
    for (const Entity entity : {alive, killed}) {
        manager.add(entity, Position{.x = 0, .y = 0});
        manager.add(entity, Velocity{.dx = 0, .dy = 0});
    }
    manager.kill(killed);

    int count = 0;
    manager.query<Position, Velocity>().each([&](Entity, Position&, Velocity&) { ++count; });

    EXPECT_EQ(count, 1);
}

TEST(EntityManager, QueryAllowsMutatingComponents) {
    EntityManager manager = make_manager();
    const Entity entity = manager.spawn();
    manager.add(entity, Position{.x = 0, .y = 0});
    manager.add(entity, Velocity{.dx = 2, .dy = 3});

    manager.query<Position, Velocity>().each([](Entity, Position& position, Velocity& velocity) {
        position.x += velocity.dx;
        position.y += velocity.dy;
    });

    EXPECT_EQ(manager.get<Position>(entity).x, 2);
    EXPECT_EQ(manager.get<Position>(entity).y, 3);
}

TEST(EntityManager, QueryOfEmptyPoolVisitsNothing) {
    EntityManager manager = make_manager();

    int count = 0;
    manager.query<Position>().each([&](Entity, Position&) { ++count; });

    EXPECT_EQ(count, 0);
}

TEST(EntityManager, EntityManagersAreIndependent) {
    EntityManager first = make_manager();
    EntityManager second;
    second.register_component<Position>();
    const Entity in_first = first.spawn();
    first.add(in_first, Position{.x = 1, .y = 1});

    int count = 0;
    second.query<Position>().each([&](Entity, Position&) { ++count; });

    EXPECT_EQ(count, 0);
    EXPECT_FALSE(second.alive(in_first));
}
