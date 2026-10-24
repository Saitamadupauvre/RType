#include <gtest/gtest.h>

#include <string>
#include <type_traits>
#include <utility>

#include "engine/audio/PluginLoader.hpp"
#include "engine/core/DynamicLibrary.hpp"

using engine::audio::AudioPlugin;
using engine::audio::load_audio_plugin;
using engine::core::DynamicLibrary;

namespace {

using CountFunction = int (*)();

int destroyed_count(const DynamicLibrary& probe) {
    const auto count = probe.symbol<CountFunction>("fixture_destroyed_count");
    return count != nullptr ? count() : -1;
}

void expect_rejected(const char* path, const std::string& reason) {
    const auto result = load_audio_plugin(path);

    EXPECT_FALSE(result.plugin.has_value());
    EXPECT_NE(result.error.find(path), std::string::npos) << result.error;
    EXPECT_NE(result.error.find(reason), std::string::npos) << result.error;
}

} // namespace

TEST(AudioPluginLoader, LoadsValidPlugin) {
    auto result = load_audio_plugin(VALID_PLUGIN_PATH);

    if (!result.plugin.has_value()) {
        FAIL() << result.error;
    }
    EXPECT_TRUE(result.error.empty());
    EXPECT_TRUE(result.plugin->backend().open());
}

TEST(AudioPluginLoader, BackendIsDestroyedByThePlugin) {
    const DynamicLibrary probe(VALID_PLUGIN_PATH);
    ASSERT_TRUE(probe) << probe.error();
    const int before = destroyed_count(probe);

    {
        const auto result = load_audio_plugin(VALID_PLUGIN_PATH);
        ASSERT_TRUE(result.plugin.has_value()) << result.error;
    }

    EXPECT_EQ(destroyed_count(probe), before + 1);
}

TEST(AudioPluginLoader, MovedPluginOwnsTheBackend) {
    const DynamicLibrary probe(VALID_PLUGIN_PATH);
    ASSERT_TRUE(probe) << probe.error();
    const int before = destroyed_count(probe);

    {
        auto result = load_audio_plugin(VALID_PLUGIN_PATH);
        if (!result.plugin.has_value()) {
            FAIL() << result.error;
        }
        AudioPlugin moved(std::move(*result.plugin));
        result.plugin.reset();
        EXPECT_EQ(destroyed_count(probe), before);
        EXPECT_TRUE(moved.backend().open());
    }

    EXPECT_EQ(destroyed_count(probe), before + 1);
}

TEST(AudioPluginLoader, PluginCannotBeCopiedOrMoveAssigned) {
    static_assert(!std::is_copy_constructible_v<AudioPlugin>);
    static_assert(!std::is_copy_assignable_v<AudioPlugin>);
    static_assert(!std::is_move_assignable_v<AudioPlugin>);
    static_assert(std::is_nothrow_move_constructible_v<AudioPlugin>);
    SUCCEED();
}

TEST(AudioPluginLoader, RejectsMissingFile) {
    expect_rejected("no-such-directory/no-such-plugin", "cannot open");
}

TEST(AudioPluginLoader, RejectsWrongApiVersion) {
    expect_rejected(WRONG_VERSION_PLUGIN_PATH, "API version");
}

TEST(AudioPluginLoader, RejectsMissingEntryPoint) {
    expect_rejected(MISSING_CREATE_PLUGIN_PATH, "engine_audio_create");
}

TEST(AudioPluginLoader, RejectsNullBackend) {
    expect_rejected(NULL_BACKEND_PLUGIN_PATH, "returned null");
}

TEST(AudioPluginLoader, LoadsRaylibPluginWithoutOpeningTheDevice) {
    auto result = load_audio_plugin(RAYLIB_PLUGIN_PATH);

    if (!result.plugin.has_value()) {
        FAIL() << result.error;
    }
    auto& audio = result.plugin->backend();
    EXPECT_FALSE(audio.is_open());
    EXPECT_EQ(audio.load_sound("sounds/shot.wav"), engine::audio::SoundId{});
    EXPECT_EQ(audio.load_music("music/stage.ogg"), engine::audio::MusicId{});
}
