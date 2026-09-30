#include <gtest/gtest.h>

#include <set>
#include <stdexcept>
#include <string>

#include "engine/core/Registry.hpp"

using engine::core::ComponentAlreadyRegistered;
using engine::core::ComponentNotRegistered;
using engine::core::DistinctTypes;
using engine::core::Entity;
using engine::core::Registry;

namespace {

struct Position {
    int x;
    int y;
};

struct Velocity {
    int dx;
    int dy;
};

Registry make_registry() {
    Registry registry;
    registry.register_component<Position>();
    registry.register_component<Velocity>();
    return registry;
}

} // namespace

static_assert(DistinctTypes<Position, Velocity>);
static_assert(!DistinctTypes<Position, Position>);
static_assert(!DistinctTypes<Position, Velocity, Position>);

TEST(Registry, SpawnsDistinctLivingEntities) {
    Registry registry;

    const Entity first = registry.spawn();
    const Entity second = registry.spawn();

    EXPECT_NE(first, second);
    EXPECT_TRUE(registry.alive(first));
    EXPECT_TRUE(registry.alive(second));
}

TEST(Registry, KillRemovesAllComponents) {
    Registry registry = make_registry();
    const Entity entity = registry.spawn();
    registry.add(entity, Position{.x = 1, .y = 2});
    registry.add(entity, Velocity{.dx = 3, .dy = 4});

    EXPECT_TRUE(registry.kill(entity));

    EXPECT_FALSE(registry.alive(entity));
    EXPECT_THROW(static_cast<void>(registry.get<Position>(entity)), std::out_of_range);
    EXPECT_THROW(static_cast<void>(registry.get<Velocity>(entity)), std::out_of_range);
}

TEST(Registry, KillIgnoresStaleHandle) {
    Registry registry = make_registry();
    const Entity stale = registry.spawn();
    registry.kill(stale);
    const Entity fresh = registry.spawn();
    registry.add(fresh, Position{.x = 5, .y = 6});

    EXPECT_FALSE(registry.kill(stale));

    EXPECT_TRUE(registry.alive(fresh));
    EXPECT_EQ(registry.get<Position>(fresh).x, 5);
}

TEST(Registry, RecycledSlotHasNoOldComponents) {
    Registry registry = make_registry();
    const Entity old_entity = registry.spawn();
    registry.add(old_entity, Position{.x = 1, .y = 1});
    registry.kill(old_entity);

    const Entity recycled = registry.spawn();

    EXPECT_EQ(recycled.index(), old_entity.index());
    EXPECT_THROW(static_cast<void>(registry.get<Position>(recycled)), std::out_of_range);
}

TEST(Registry, AddReplacesExistingComponent) {
    Registry registry = make_registry();
    const Entity entity = registry.spawn();
    registry.add(entity, Position{.x = 1, .y = 2});

    registry.add(entity, Position{.x = 7, .y = 8});

    EXPECT_EQ(registry.get<Position>(entity).x, 7);
    EXPECT_EQ(registry.get<Position>(entity).y, 8);
}

TEST(Registry, RemoveReportsWhetherAComponentWasRemoved) {
    Registry registry = make_registry();
    const Entity entity = registry.spawn();
    registry.add(entity, Position{.x = 1, .y = 2});

    EXPECT_TRUE(registry.remove<Position>(entity));
    EXPECT_FALSE(registry.remove<Position>(entity));
    EXPECT_FALSE(registry.remove<Velocity>(entity));
}

TEST(Registry, AddThrowsOnDeadEntity) {
    Registry registry = make_registry();
    const Entity entity = registry.spawn();
    registry.kill(entity);

    EXPECT_THROW(registry.add(entity, Position{.x = 0, .y = 0}), std::invalid_argument);
}

TEST(Registry, AddThrowsWhenComponentNotRegistered) {
    Registry registry;
    const Entity entity = registry.spawn();

    EXPECT_THROW(registry.add(entity, Position{.x = 0, .y = 0}), ComponentNotRegistered);
}

TEST(Registry, UnregisteredErrorNamesTheType) {
    Registry registry;
    const Entity entity = registry.spawn();

    try {
        static_cast<void>(registry.get<Position>(entity));
        FAIL() << "expected ComponentNotRegistered";
    } catch (const ComponentNotRegistered& error) {
        EXPECT_NE(std::string(error.what()).find("Position"), std::string::npos);
    }
}

TEST(Registry, RemoveAndGetThrowWhenComponentNotRegistered) {
    Registry registry;
    const Entity entity = registry.spawn();

    EXPECT_THROW(registry.remove<Position>(entity), ComponentNotRegistered);
    EXPECT_THROW(static_cast<void>(registry.get<Position>(entity)), ComponentNotRegistered);
}

TEST(Registry, ViewThrowsWhenComponentNotRegistered) {
    Registry registry;
    registry.register_component<Position>();

    EXPECT_THROW(static_cast<void>(registry.view<Position, Velocity>()), ComponentNotRegistered);
}

TEST(Registry, RegisterTwiceThrows) {
    Registry registry;
    registry.register_component<Position>();

    EXPECT_THROW(registry.register_component<Position>(), ComponentAlreadyRegistered);
}

TEST(Registry, ViewSkipsEntitiesMissingAComponent) {
    Registry registry = make_registry();
    const Entity both = registry.spawn();
    const Entity only_position = registry.spawn();
    const Entity only_velocity = registry.spawn();
    registry.add(both, Position{.x = 1, .y = 1});
    registry.add(both, Velocity{.dx = 2, .dy = 2});
    registry.add(only_position, Position{.x = 3, .y = 3});
    registry.add(only_velocity, Velocity{.dx = 4, .dy = 4});

    std::set<Entity::Id> visited;
    registry.view<Position, Velocity>().each(
        [&](Entity entity, Position&, Velocity&) { visited.insert(entity.id()); });

    EXPECT_EQ(visited, std::set<Entity::Id>{both.id()});
}

TEST(Registry, ViewGivesSameResultWhicheverPoolIsSmaller) {
    Registry registry = make_registry();
    const Entity shared = registry.spawn();
    registry.add(shared, Position{.x = 1, .y = 1});
    registry.add(shared, Velocity{.dx = 2, .dy = 2});
    for (int i = 0; i < 3; ++i) {
        registry.add(registry.spawn(), Position{.x = i, .y = i});
    }

    int forward = 0;
    registry.view<Position, Velocity>().each([&](Entity, Position&, Velocity&) { ++forward; });
    int backward = 0;
    registry.view<Velocity, Position>().each([&](Entity, Velocity&, Position&) { ++backward; });

    EXPECT_EQ(forward, 1);
    EXPECT_EQ(backward, 1);
}

TEST(Registry, ViewDoesNotVisitKilledEntities) {
    Registry registry = make_registry();
    const Entity alive = registry.spawn();
    const Entity killed = registry.spawn();
    for (const Entity entity : {alive, killed}) {
        registry.add(entity, Position{.x = 0, .y = 0});
        registry.add(entity, Velocity{.dx = 0, .dy = 0});
    }
    registry.kill(killed);

    int count = 0;
    registry.view<Position, Velocity>().each([&](Entity, Position&, Velocity&) { ++count; });

    EXPECT_EQ(count, 1);
}

TEST(Registry, ViewAllowsMutatingComponents) {
    Registry registry = make_registry();
    const Entity entity = registry.spawn();
    registry.add(entity, Position{.x = 0, .y = 0});
    registry.add(entity, Velocity{.dx = 2, .dy = 3});

    registry.view<Position, Velocity>().each([](Entity, Position& position, Velocity& velocity) {
        position.x += velocity.dx;
        position.y += velocity.dy;
    });

    EXPECT_EQ(registry.get<Position>(entity).x, 2);
    EXPECT_EQ(registry.get<Position>(entity).y, 3);
}

TEST(Registry, ViewOfEmptyPoolVisitsNothing) {
    Registry registry = make_registry();

    int count = 0;
    registry.view<Position>().each([&](Entity, Position&) { ++count; });

    EXPECT_EQ(count, 0);
}

TEST(Registry, RegistriesAreIndependent) {
    Registry first = make_registry();
    Registry second;
    second.register_component<Position>();
    const Entity in_first = first.spawn();
    first.add(in_first, Position{.x = 1, .y = 1});

    int count = 0;
    second.view<Position>().each([&](Entity, Position&) { ++count; });

    EXPECT_EQ(count, 0);
    EXPECT_FALSE(second.alive(in_first));
}
