#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/Components.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/InputState.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"
#include "engine/script/ScriptRuntime.hpp"

namespace logger = engine::core::log;
using engine::core::Entity;
using engine::core::EntityManager;
using engine::core::InputState;
using engine::core::Position;
using engine::core::Tags;
using engine::script::ScriptRuntime;

namespace {

std::filesystem::path players_fixture(const std::string& name) {
    return std::filesystem::path(RTYPE_FIXTURES_DIR) / "scripts" / "players" / name;
}

class PlayersTest : public ::testing::Test {
protected:
    std::shared_ptr<logger::MemorySink> _sink = std::make_shared<logger::MemorySink>();
    logger::ScopedSink _scope{_sink, logger::Level::Trace};
    EntityManager _world;
    ScriptRuntime _runtime;

    void load(const std::string& fixture) {
        _runtime.attach_world(_world);
        ASSERT_TRUE(_runtime.load_scripts(players_fixture(fixture)));
    }

    [[nodiscard]] std::vector<Entity> tagged(const std::string& label) {
        std::vector<Entity> entities;
        _world.query<Tags>().each([&](Entity entity, const Tags& tags) {
            if (tags.has(label)) {
                entities.push_back(entity);
            }
        });
        return entities;
    }

    [[nodiscard]] std::vector<Entity> ships_owned_by(std::uint32_t player_id) {
        std::vector<Entity> owned;
        for (const Entity ship : tagged("ship")) {
            if (_runtime.entity_owner(ship) == player_id) {
                owned.push_back(ship);
            }
        }
        return owned;
    }

    [[nodiscard]] float y_of(Entity entity) { return _world.get<Position>(entity).pos.y; }
    [[nodiscard]] float x_of(Entity entity) { return _world.get<Position>(entity).pos.x; }

    [[nodiscard]] bool logged(logger::Level level, const std::string& part) const {
        return std::ranges::any_of(_sink->records(), [&](const auto& record) {
            return record.level == level && record.message.find(part) != std::string::npos;
        });
    }
};

InputState moving_right() {
    return InputState{.keys = {engine::core::Key::Right}, .mouse_buttons = {}, .pointer = {}};
}

} // namespace

TEST_F(PlayersTest, JoinCallsOnPlayerJoinedWithTheLowestFreeSlot) {
    load("callbacks");

    EXPECT_TRUE(_runtime.player_joined(7));
    EXPECT_TRUE(_runtime.player_joined(9));

    ASSERT_EQ(ships_owned_by(7).size(), 1U);
    ASSERT_EQ(ships_owned_by(9).size(), 1U);
    EXPECT_FLOAT_EQ(y_of(ships_owned_by(7).front()), 0.F);
    EXPECT_FLOAT_EQ(y_of(ships_owned_by(9).front()), 100.F);
}

TEST_F(PlayersTest, DuplicateJoinIsRejected) {
    load("callbacks");
    ASSERT_TRUE(_runtime.player_joined(1));

    EXPECT_FALSE(_runtime.player_joined(1));

    EXPECT_EQ(tagged("ship").size(), 1U);
    EXPECT_TRUE(logged(logger::Level::Warn, "already connected"));
}

TEST_F(PlayersTest, SlotIsReusedAfterLeave) {
    load("callbacks");
    _runtime.player_joined(1);
    _runtime.player_joined(2);
    _runtime.player_left(1);
    _runtime.flush_destroyed();

    _runtime.player_joined(3);

    ASSERT_EQ(ships_owned_by(3).size(), 1U);
    EXPECT_FLOAT_EQ(y_of(ships_owned_by(3).front()), 0.F);
}

TEST_F(PlayersTest, InputReachesOnlyEntitiesOwnedByThatPlayer) {
    load("callbacks");
    _runtime.player_joined(1);
    _runtime.player_joined(2);

    _runtime.set_player_input(1, moving_right());
    _runtime.update(1.F);

    EXPECT_FLOAT_EQ(x_of(ships_owned_by(1).front()), 1.F);
    EXPECT_FLOAT_EQ(x_of(ships_owned_by(2).front()), 0.F);
}

TEST_F(PlayersTest, InputForUnknownPlayerIsIgnored) {
    load("callbacks");
    _runtime.player_joined(1);

    _runtime.set_player_input(5, moving_right());
    _runtime.update(1.F);

    EXPECT_FLOAT_EQ(x_of(ships_owned_by(1).front()), 0.F);
}

TEST_F(PlayersTest, LeaveCallsOnPlayerLeft) {
    load("callbacks");
    _runtime.player_joined(4);

    EXPECT_TRUE(_runtime.player_left(4));
    _runtime.flush_destroyed();

    EXPECT_TRUE(tagged("ship").empty());
    ASSERT_EQ(tagged("marker").size(), 1U);
    EXPECT_FLOAT_EQ(y_of(tagged("marker").front()), 4.F);
}

TEST_F(PlayersTest, UnknownPlayerLeavingIsRejected) {
    load("callbacks");

    EXPECT_FALSE(_runtime.player_left(3));

    EXPECT_TRUE(tagged("marker").empty());
    EXPECT_TRUE(logged(logger::Level::Warn, "unknown player 3"));
}

TEST_F(PlayersTest, LeaveWithoutCallbackDestroysOwnedEntities) {
    load("default");
    _runtime.player_joined(1);
    _runtime.player_joined(2);

    _runtime.player_left(1);
    _runtime.flush_destroyed();

    EXPECT_TRUE(ships_owned_by(1).empty());
    EXPECT_EQ(tagged("ship").size(), 1U);
}

TEST_F(PlayersTest, FailingCallbacksAreLoggedAndPlayersStillTracked) {
    load("broken");

    EXPECT_FALSE(_runtime.player_joined(1));
    EXPECT_TRUE(logged(logger::Level::Error, "join failed for 1"));
    EXPECT_TRUE(_runtime.run_string("assert(#players() == 1)", "test"));

    EXPECT_FALSE(_runtime.player_left(1));
    EXPECT_TRUE(logged(logger::Level::Error, "leave failed for 1"));
    EXPECT_TRUE(_runtime.run_string("assert(#players() == 0)", "test"));
}

TEST_F(PlayersTest, PlayerHandleExposesIdSlotAndConnection) {
    load("default");
    _runtime.player_joined(12);

    EXPECT_TRUE(_runtime.run_string(R"(
        local player = find_player(12)
        assert(player.id == 12)
        assert(player.slot == 0)
        assert(player.connected)
        assert(#player:entities() == 1)
        assert(players()[1] == player)
        assert(find_player(99) == nil)
    )",
                                    "test"));
}

TEST_F(PlayersTest, OwnerCanBeSetAndClearedFromLua) {
    load("default");
    _runtime.player_joined(1);

    EXPECT_TRUE(_runtime.run_string(R"(
        local ship = spawn("ship", { owner = find_player(1) })
        assert(ship.owner == find_player(1))
        ship.owner = nil
        assert(ship.owner == nil)
    )",
                                    "test"));
}

TEST_F(PlayersTest, OwnerMustBeAPlayer) {
    load("default");

    EXPECT_FALSE(_runtime.run_string("spawn('ship', { owner = 5 })", "test"));

    EXPECT_TRUE(logged(logger::Level::Error, "must be a player or nil"));
}

TEST_F(PlayersTest, OwnerReadsNilOnceThePlayerHasLeft) {
    load("callbacks");
    _runtime.player_joined(1);
    _runtime.run_string("kept = spawn('marker')", "test");
    _runtime.run_string("kept.owner = find_player(1)", "test");
    _runtime.run_string("handle = find_player(1)", "test");

    _runtime.player_left(1);

    EXPECT_TRUE(_runtime.run_string("assert(kept.owner == nil)", "test"));
    EXPECT_TRUE(_runtime.run_string("assert(not handle.connected and handle.slot == nil)", "test"));
}

TEST_F(PlayersTest, EntityTypesListsEntityDeclarationsInOrder) {
    load("callbacks");

    EXPECT_EQ(_runtime.entity_types(), (std::vector<std::string>{"marker", "ship"}));
}

TEST_F(PlayersTest, EntityTypeNamesTheDeclaration) {
    load("callbacks");
    const auto ship = _runtime.spawn("ship");

    ASSERT_TRUE(ship.has_value());
    EXPECT_EQ(_runtime.entity_type(*ship), "ship");
    EXPECT_FALSE(_runtime.entity_type(Entity{999}).has_value());
}
