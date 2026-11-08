#include <gtest/gtest.h>

#include <filesystem>

#include "Options.hpp"

using engine::app::Mode;
using runtime::launch_options;
using runtime::RuntimeConfig;
using runtime::window_title;

namespace {

namespace fs = std::filesystem;

const fs::path fixtures = fs::path(TEST_FIXTURES_DIR) / "runtime";
const runtime::PluginFiles plugins{.graphics = "graphics.so", .audio = "audio.so"};

} // namespace

TEST(RuntimeOptions, TitleIsTheGameName) {
    EXPECT_EQ(window_title(fixtures / "named"), "Space Game");
}

TEST(RuntimeOptions, TitleFallsBackToTheFolderName) {
    EXPECT_EQ(window_title(fixtures / "unnamed-game"), "unnamed-game");
    EXPECT_EQ(window_title(fixtures / "unnamed-game/"), "unnamed-game");
}

TEST(RuntimeOptions, TitleOfAMissingFolderIsItsName) {
    EXPECT_EQ(window_title(fixtures / "missing"), "missing");
}

TEST(RuntimeOptions, GameDefaultsToTheExecutableFolder) {
    const auto options = launch_options(RuntimeConfig{.mode = Mode::solo}, "/opt/game", plugins);

    EXPECT_EQ(options.game, fs::path("/opt/game"));
}

TEST(RuntimeOptions, GameFolderOptionWins) {
    const auto options = launch_options(
        RuntimeConfig{.mode = Mode::solo, .game = (fixtures / "named").string()}, "/opt", plugins);

    EXPECT_EQ(options.game, fixtures / "named");
    EXPECT_EQ(options.window.title, "Space Game");
}

TEST(RuntimeOptions, PluginsAreNextToTheExecutable) {
    const auto options = launch_options(RuntimeConfig{.mode = Mode::solo}, "/opt/game", plugins);

    EXPECT_EQ(options.graphics_plugin, fs::path("/opt/game/plugins/graphics.so"));
    EXPECT_EQ(options.audio_plugin, fs::path("/opt/game/plugins/audio.so"));
}

TEST(RuntimeOptions, CarriesTheModeNetworkAndScene) {
    const auto options = launch_options(RuntimeConfig{.mode = Mode::host,
                                                      .server_ip = "10.0.0.2",
                                                      .port = 4242,
                                                      .max_players = 2,
                                                      .scene_file = "/tmp/level.json"},
                                        "/opt", plugins);

    EXPECT_EQ(options.mode, Mode::host);
    EXPECT_EQ(options.server_ip, "10.0.0.2");
    EXPECT_EQ(options.port, 4242);
    EXPECT_EQ(options.max_players, 2U);
    EXPECT_EQ(options.start_scene_file, fs::path("/tmp/level.json"));
}

TEST(RuntimeOptions, VolumesBecomeLevels) {
    const auto options = launch_options(
        RuntimeConfig{.mode = Mode::solo, .music_volume = 30, .effects_volume = 0, .muted = true},
        "/opt", plugins);

    EXPECT_FLOAT_EQ(options.audio.music_volume, 0.3F);
    EXPECT_FLOAT_EQ(options.audio.effects_volume, 0.F);
    EXPECT_TRUE(options.audio.muted);
}
