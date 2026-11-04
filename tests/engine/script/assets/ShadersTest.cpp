#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
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
using engine::core::Shader;
using engine::script::ScriptRuntime;
using engine::script::ServerApi;

namespace {

std::filesystem::path shaders_fixture(const std::string& name) {
    return std::filesystem::path(RTYPE_FIXTURES_DIR) / "scripts" / "shaders" / name;
}

class ShadersTest : public ::testing::Test {
protected:
    std::shared_ptr<logger::MemorySink> _sink = std::make_shared<logger::MemorySink>();
    logger::ScopedSink _scope{_sink, logger::Level::Trace};
    EntityManager _world;
    ScriptRuntime _runtime = ScriptRuntime::builder().with<ServerApi>().build();

    void SetUp() override { _runtime.attach_world(_world); }

    Entity spawn_backdrop() {
        const auto entity = _runtime.spawn("backdrop");
        if (!entity) {
            throw std::runtime_error("spawn failed: backdrop");
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

TEST_F(ShadersTest, ListsDeclaredShadersAndEntityShadersOnceInAscendingOrder) {
    ASSERT_TRUE(_runtime.load_scripts(shaders_fixture("declared")));

    EXPECT_EQ(_runtime.shader_names(),
              (std::vector<std::string>{"shaders/fx/wave", "shaders/nebula", "shaders/space"}));
}

TEST_F(ShadersTest, KeepsShadersApartFromTextures) {
    ASSERT_TRUE(_runtime.load_scripts(shaders_fixture("declared")));

    EXPECT_TRUE(_runtime.texture_names().empty());
}

TEST_F(ShadersTest, NothingIsDeclaredWithoutScripts) {
    EXPECT_TRUE(_runtime.shader_names().empty());
}

TEST_F(ShadersTest, EntityStartsWithItsDeclaredShader) {
    ASSERT_TRUE(_runtime.load_scripts(shaders_fixture("declared")));

    EXPECT_EQ(_world.get<Shader>(spawn_backdrop()).path, "shaders/nebula");
}

TEST_F(ShadersTest, ScriptsMaySwitchToADeclaredShaderOrToNone) {
    ASSERT_TRUE(_runtime.load_scripts(shaders_fixture("declared")));
    const Entity backdrop = spawn_backdrop();

    ASSERT_TRUE(_runtime.call(backdrop, "use_wave"));
    EXPECT_EQ(_world.get<Shader>(backdrop).path, "shaders/fx/wave");
    ASSERT_TRUE(_runtime.call(backdrop, "use_nothing"));
    EXPECT_TRUE(_world.get<Shader>(backdrop).path.empty());
}

TEST_F(ShadersTest, AnUndeclaredShaderRaisesAnErrorAndKeepsTheShader) {
    ASSERT_TRUE(_runtime.load_scripts(shaders_fixture("declared")));
    const Entity backdrop = spawn_backdrop();

    EXPECT_FALSE(_runtime.call(backdrop, "use_unknown"));

    EXPECT_EQ(_world.get<Shader>(backdrop).path, "shaders/nebula");
    EXPECT_EQ(count_errors("unknown shader 'shaders/unknown', declare it with shaders"), 1U);
}

TEST_F(ShadersTest, AShaderNameIsNotADeclaredTexture) {
    ASSERT_TRUE(_runtime.load_scripts(shaders_fixture("declared")));
    const Entity backdrop = spawn_backdrop();

    EXPECT_FALSE(_runtime.call(backdrop, "use_texture"));

    EXPECT_EQ(count_errors("unknown texture 'shaders/space', declare it with textures"), 1U);
}

TEST_F(ShadersTest, DeclaringAfterLoadingFails) {
    ASSERT_TRUE(_runtime.load_scripts(shaders_fixture("declared")));

    EXPECT_FALSE(_runtime.run_string(R"(shaders { "shaders/late" })", "late"));

    EXPECT_EQ(_runtime.shader_names().size(), 3U);
    EXPECT_EQ(count_errors("shaders can only be called while the scripts load"), 1U);
}

TEST_F(ShadersTest, RejectsAPathLeavingTheAssetsFolderAndKeepsNoNameOfThatCall) {
    EXPECT_FALSE(_runtime.load_scripts(shaders_fixture("invalid_name")));

    EXPECT_TRUE(_runtime.shader_names().empty());
    EXPECT_EQ(count_errors("'../shaders/secret' is not a valid file name"), 1U);
}

TEST_F(ShadersTest, AnEntityWithAnInvalidShaderFailsTheLoad) {
    EXPECT_FALSE(_runtime.load_scripts(shaders_fixture("invalid_shader")));

    EXPECT_TRUE(_runtime.shader_names().empty());
    EXPECT_EQ(count_errors("entity 'backdrop' has an invalid shader '/etc/secret'"), 1U);
}
