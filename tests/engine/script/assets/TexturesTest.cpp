#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "engine/core/ecs/Components.hpp"
#include "engine/core/ecs/EntityManager.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"
#include "engine/script/ScriptRuntime.hpp"
#include "engine/script/ServerApi.hpp"

namespace logger = engine::core::log;
using engine::core::Entity;
using engine::core::EntityManager;
using engine::core::Sprite;
using engine::script::ScriptRuntime;
using engine::script::ServerApi;

namespace {

std::filesystem::path textures_fixture(const std::string& name) {
    return std::filesystem::path(TEST_FIXTURES_DIR) / "scripts" / "textures" / name;
}

class TexturesTest : public ::testing::Test {
protected:
    std::shared_ptr<logger::MemorySink> _sink = std::make_shared<logger::MemorySink>();
    logger::ScopedSink _scope{_sink, logger::Level::Trace};
    EntityManager _world;
    ScriptRuntime _runtime = ScriptRuntime::builder().with<ServerApi>().build();

    void SetUp() override { _runtime.attach_world(_world); }

    Entity spawn_ship() {
        const auto entity = _runtime.spawn("ship");
        if (!entity) {
            throw std::runtime_error("spawn failed: ship");
        }
        return *entity;
    }

    [[nodiscard]] std::size_t count_errors(const std::string& part) const {
        return static_cast<std::size_t>(
            std::ranges::count_if(_sink->records(), [&](const logger::MemoryRecord& record) {
                return record.level == logger::Level::Error &&
                       record.message.find(part) != std::string::npos;
            }));
    }
};

} // namespace

TEST_F(TexturesTest, ListsDeclaredTexturesAndEntitySpritesOnceInAscendingOrder) {
    ASSERT_TRUE(_runtime.load_scripts(textures_fixture("declared")));

    EXPECT_EQ(_runtime.texture_names(),
              (std::vector<std::string>{"sprites/boss/core.png", "sprites/ship.png",
                                        "sprites/shot.png"}));
}

TEST_F(TexturesTest, NothingIsDeclaredWithoutScripts) {
    EXPECT_TRUE(_runtime.texture_names().empty());
}

TEST_F(TexturesTest, EntityStartsWithItsDeclaredSprite) {
    ASSERT_TRUE(_runtime.load_scripts(textures_fixture("declared")));

    EXPECT_EQ(_world.get<Sprite>(spawn_ship()).path, "sprites/ship.png");
}

TEST_F(TexturesTest, ScriptsMaySwitchToADeclaredTextureOrToNone) {
    ASSERT_TRUE(_runtime.load_scripts(textures_fixture("declared")));
    const Entity ship = spawn_ship();

    ASSERT_TRUE(_runtime.call(ship, "wear_boss"));
    EXPECT_EQ(_world.get<Sprite>(ship).path, "sprites/boss/core.png");
    ASSERT_TRUE(_runtime.call(ship, "wear_nothing"));
    EXPECT_TRUE(_world.get<Sprite>(ship).path.empty());
}

TEST_F(TexturesTest, AnUndeclaredTextureRaisesAnErrorAndKeepsTheSprite) {
    ASSERT_TRUE(_runtime.load_scripts(textures_fixture("declared")));
    const Entity ship = spawn_ship();

    EXPECT_FALSE(_runtime.call(ship, "wear_unknown"));

    EXPECT_EQ(_world.get<Sprite>(ship).path, "sprites/ship.png");
    EXPECT_EQ(count_errors("unknown texture 'sprites/unknown.png', declare it with textures"), 1U);
}

TEST_F(TexturesTest, DeclaringAfterLoadingFails) {
    ASSERT_TRUE(_runtime.load_scripts(textures_fixture("declared")));

    EXPECT_FALSE(_runtime.run_string(R"(textures { "sprites/late.png" })", "late"));

    EXPECT_EQ(_runtime.texture_names().size(), 3U);
    EXPECT_EQ(count_errors("textures can only be called while the scripts load"), 1U);
}

TEST_F(TexturesTest, RejectsAPathLeavingTheAssetsFolderAndKeepsNoNameOfThatCall) {
    EXPECT_FALSE(_runtime.load_scripts(textures_fixture("invalid_name")));

    EXPECT_TRUE(_runtime.texture_names().empty());
    EXPECT_EQ(count_errors("'../secret.png' is not a valid file name"), 1U);
}

TEST_F(TexturesTest, RejectsANameLongerThan63Bytes) {
    EXPECT_FALSE(_runtime.load_scripts(textures_fixture("too_long")));

    EXPECT_TRUE(_runtime.texture_names().empty());
    EXPECT_EQ(count_errors("is not a valid file name"), 1U);
}

TEST_F(TexturesTest, RejectsADeclarationThatIsNotAList) {
    EXPECT_FALSE(_runtime.load_scripts(textures_fixture("not_a_list")));

    EXPECT_EQ(count_errors("textures expects a list of file names"), 1U);
}

TEST_F(TexturesTest, AnEntityWithAnInvalidSpriteFailsTheLoad) {
    EXPECT_FALSE(_runtime.load_scripts(textures_fixture("invalid_sprite")));

    EXPECT_TRUE(_runtime.texture_names().empty());
    EXPECT_EQ(count_errors("entity 'enemy' has an invalid sprite '/etc/secret.png'"), 1U);
}
