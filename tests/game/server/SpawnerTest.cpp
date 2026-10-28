#include <gtest/gtest.h>

#include <cstddef>
#include <filesystem>
#include <format>
#include <string>
#include <vector>

#include "engine/core/ComponentPhysics.hpp"
#include "engine/core/Components.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/Vec2.hpp"
#include "engine/script/ScriptRuntime.hpp"
#include "engine/script/ServerApi.hpp"

using engine::core::Entity;
using engine::core::EntityManager;
using engine::core::Position;
using engine::core::Tags;
using engine::core::Vec2;
using engine::script::ScriptRuntime;
using engine::script::ServerApi;

namespace {

constexpr float spawn_interval = 1.5F;
constexpr float world_width = 1920.F;
constexpr float bydo_height = 72.F;
constexpr float max_y = 1080.F - bydo_height;
constexpr std::size_t waves = 20;

const std::filesystem::path game_scripts =
    std::filesystem::path(RTYPE_FIXTURES_DIR) / "game" / "server" / "game";

class Game {
public:
    explicit Game(int seed) {
        _runtime.attach_world(_world);
        _loaded = _runtime.load_scripts(game_scripts) &&
                  _runtime.spawn("bydo_spawner").has_value() &&
                  _runtime.run_string(std::format("math.randomseed({})", seed), "seed");
    }

    [[nodiscard]] bool loaded() const { return _loaded; }

    void tick(float dt) {
        _runtime.update(dt);
        _runtime.flush_destroyed();
    }

    [[nodiscard]] std::vector<Vec2> bydos() {
        std::vector<Vec2> found;
        _world.query<Tags, Position>().each(
            [&](Entity, const Tags& tags, const Position& position) {
                if (tags.has("enemy")) {
                    found.push_back(position.pos);
                }
            });
        return found;
    }

    [[nodiscard]] std::vector<Vec2> just_spawned() {
        std::vector<Vec2> found;
        for (const Vec2 bydo : bydos()) {
            if (bydo.x == world_width) {
                found.push_back(bydo);
            }
        }
        return found;
    }

    [[nodiscard]] std::vector<float> spawn_heights(std::size_t count) {
        std::vector<float> heights;
        for (std::size_t wave = 0; wave < count; ++wave) {
            tick(spawn_interval);
            for (const Vec2 bydo : just_spawned()) {
                heights.push_back(bydo.y);
            }
        }
        return heights;
    }

private:
    EntityManager _world;
    ScriptRuntime _runtime = ScriptRuntime::builder().with<ServerApi>().build();
    ServerApi& _server = _runtime.api<ServerApi>();
    bool _loaded = false;
};

} // namespace

TEST(SpawnerTest, NoBydoBeforeTheFirstInterval) {
    Game game(1);
    ASSERT_TRUE(game.loaded());
    game.tick(1.F);
    EXPECT_TRUE(game.bydos().empty());
}

TEST(SpawnerTest, SpawnsOneBydoPerInterval) {
    Game game(1);
    ASSERT_TRUE(game.loaded());
    game.tick(1.F);
    game.tick(0.5F);
    EXPECT_EQ(game.bydos().size(), 1U);
    game.tick(spawn_interval);
    EXPECT_EQ(game.bydos().size(), 2U);
}

TEST(SpawnerTest, CatchesUpWhenATickSpansSeveralIntervals) {
    Game game(1);
    ASSERT_TRUE(game.loaded());
    game.tick(spawn_interval * 3.F);
    EXPECT_EQ(game.bydos().size(), 3U);
}

TEST(SpawnerTest, BydosAppearAtTheRightEdgeFullyInsideTheScreenHeight) {
    Game game(7);
    ASSERT_TRUE(game.loaded());
    for (std::size_t wave = 0; wave < waves; ++wave) {
        game.tick(spawn_interval);
        const auto spawned = game.just_spawned();
        ASSERT_EQ(spawned.size(), 1U);
        EXPECT_GE(spawned.front().y, 0.F);
        EXPECT_LE(spawned.front().y, max_y);
    }
}

TEST(SpawnerTest, SameSeedSpawnsAtTheSameHeights) {
    Game first(42);
    Game second(42);
    ASSERT_TRUE(first.loaded() && second.loaded());
    EXPECT_EQ(first.spawn_heights(waves), second.spawn_heights(waves));
}

TEST(SpawnerTest, DifferentSeedsSpawnAtDifferentHeights) {
    Game first(42);
    Game second(43);
    ASSERT_TRUE(first.loaded() && second.loaded());
    EXPECT_NE(first.spawn_heights(waves), second.spawn_heights(waves));
}
