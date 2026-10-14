#include <gtest/gtest.h>

#include <string>
#include <type_traits>
#include <utility>

#include "engine/core/DynamicLibrary.hpp"
#include "engine/graphics/PluginLoader.hpp"

using engine::core::DynamicLibrary;
using engine::graphics::GraphicsPlugin;
using engine::graphics::load_graphics_plugin;

namespace {

using CountFunction = int (*)();

int destroyed_count(const DynamicLibrary& probe) {
    const auto count = probe.symbol<CountFunction>("fixture_destroyed_count");
    return count != nullptr ? count() : -1;
}

void expect_rejected(const char* path, const std::string& reason) {
    const auto result = load_graphics_plugin(path);

    EXPECT_FALSE(result.plugin.has_value());
    EXPECT_NE(result.error.find(path), std::string::npos) << result.error;
    EXPECT_NE(result.error.find(reason), std::string::npos) << result.error;
}

} // namespace

TEST(PluginLoader, LoadsValidPlugin) {
    auto result = load_graphics_plugin(VALID_PLUGIN_PATH);

    if (!result.plugin.has_value()) {
        FAIL() << result.error;
    }
    GraphicsPlugin& plugin = *result.plugin;
    EXPECT_TRUE(result.error.empty());
    EXPECT_EQ(plugin.backend().renderer().virtual_size().x, 1920.F);
}

TEST(PluginLoader, BackendIsDestroyedByThePlugin) {
    const DynamicLibrary probe(VALID_PLUGIN_PATH);
    ASSERT_TRUE(probe) << probe.error();
    const int before = destroyed_count(probe);

    {
        const auto result = load_graphics_plugin(VALID_PLUGIN_PATH);
        ASSERT_TRUE(result.plugin.has_value()) << result.error;
    }

    EXPECT_EQ(destroyed_count(probe), before + 1);
}

TEST(PluginLoader, MovedPluginOwnsTheBackend) {
    const DynamicLibrary probe(VALID_PLUGIN_PATH);
    ASSERT_TRUE(probe) << probe.error();
    const int before = destroyed_count(probe);

    {
        auto result = load_graphics_plugin(VALID_PLUGIN_PATH);
        if (!result.plugin.has_value()) {
            FAIL() << result.error;
        }
        GraphicsPlugin moved(std::move(*result.plugin));
        result.plugin.reset();
        EXPECT_EQ(destroyed_count(probe), before);
        EXPECT_EQ(moved.backend().renderer().virtual_size().y, 1080.F);
    }

    EXPECT_EQ(destroyed_count(probe), before + 1);
}

TEST(PluginLoader, PluginCannotBeCopiedOrMoveAssigned) {
    static_assert(!std::is_copy_constructible_v<GraphicsPlugin>);
    static_assert(!std::is_copy_assignable_v<GraphicsPlugin>);
    static_assert(!std::is_move_assignable_v<GraphicsPlugin>);
    static_assert(std::is_nothrow_move_constructible_v<GraphicsPlugin>);
    SUCCEED();
}

TEST(PluginLoader, RejectsMissingFile) {
    expect_rejected("no-such-directory/no-such-plugin", "cannot open");
}

TEST(PluginLoader, RejectsWrongApiVersion) {
    expect_rejected(WRONG_VERSION_PLUGIN_PATH, "API version");
}

TEST(PluginLoader, RejectsMissingEntryPoint) {
    expect_rejected(MISSING_CREATE_PLUGIN_PATH, "engine_graphics_create");
}

TEST(PluginLoader, RejectsNullBackend) {
    expect_rejected(NULL_BACKEND_PLUGIN_PATH, "returned null");
}
