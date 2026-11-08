#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include "engine/core/ecs/Components.hpp"
#include "engine/core/ecs/EntityManager.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"
#include "engine/script/ScriptRuntime.hpp"
#include "engine/script/ServerApi.hpp"

namespace logger = engine::core::log;
using engine::core::Entity;
using engine::core::EntityManager;
using engine::core::Layer;
using engine::script::ScriptRuntime;
using engine::script::ServerApi;

namespace {

class LayerFieldsTest : public ::testing::Test {
protected:
    std::shared_ptr<logger::MemorySink> _sink = std::make_shared<logger::MemorySink>();
    logger::ScopedSink _scope{_sink, logger::Level::Trace};
    EntityManager _world;
    ScriptRuntime _runtime = ScriptRuntime::builder().with<ServerApi>().build();

    void SetUp() override {
        _runtime.attach_world(_world);
        ASSERT_TRUE(
            _runtime.load_scripts(std::filesystem::path(TEST_FIXTURES_DIR) / "scripts" / "layer"));
        ASSERT_TRUE(_runtime.run_string("backdrop = spawn('backdrop')", "setup"));
    }

    [[nodiscard]] bool logged_error(const std::string& part) const {
        return std::ranges::any_of(_sink->records(), [&](const auto& record) {
            return record.level == logger::Level::Error &&
                   record.message.find(part) != std::string::npos;
        });
    }

    [[nodiscard]] std::optional<std::int32_t> z_index_of_backdrop() {
        std::optional<std::int32_t> found;
        _world.query<Layer>().each([&](Entity, const Layer& layer) { found = layer.z_index; });
        return found;
    }
};

} // namespace

TEST_F(LayerFieldsTest, DeclarationFillsTheLayer) {
    EXPECT_EQ(z_index_of_backdrop(), -10);
    EXPECT_TRUE(_runtime.run_string("assert(backdrop.z_index == -10)", "test"));
}

TEST_F(LayerFieldsTest, EntityWithoutZIndexHasNoLayerAndReadsNil) {
    EXPECT_TRUE(
        _runtime.run_string("local ship = spawn('ship') assert(ship.z_index == nil)", "test"));
    int layers = 0;
    _world.query<Layer>().each([&](Entity, const Layer&) { ++layers; });
    EXPECT_EQ(layers, 1);
}

TEST_F(LayerFieldsTest, ScriptsChangeTheZIndexAndOverrideItOnSpawn) {
    ASSERT_TRUE(_runtime.run_string("backdrop.z_index = 3", "test"));
    EXPECT_EQ(z_index_of_backdrop(), 3);
    EXPECT_TRUE(_runtime.run_string(
        "local ship = spawn('ship', { z_index = 7 }) assert(ship.z_index == 7)", "test"));
}

TEST_F(LayerFieldsTest, RejectsAZIndexThatIsNotAnInteger) {
    EXPECT_FALSE(_runtime.run_string("backdrop.z_index = 1.5", "test"));
    EXPECT_FALSE(_runtime.run_string("backdrop.z_index = 'front'", "test"));
    EXPECT_TRUE(logged_error("z_index"));
    EXPECT_EQ(z_index_of_backdrop(), -10);
}

TEST_F(LayerFieldsTest, RejectsAZIndexOutOfRange) {
    EXPECT_FALSE(_runtime.run_string("backdrop.z_index = 1 << 40", "test"));
    EXPECT_TRUE(logged_error("z_index"));
    EXPECT_EQ(z_index_of_backdrop(), -10);
}
