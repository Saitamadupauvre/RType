#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <set>
#include <string>
#include <system_error>
#include <vector>

#include "../graphics/fixtures/NullRenderer.hpp"
#include "engine/app/ClientApp.hpp"
#include "engine/app/ServerApp.hpp"
#include "engine/audio/IAudio.hpp"
#include "engine/core/math/Camera.hpp"
#include "engine/core/time/ManualClock.hpp"
#include "engine/graphics/IInput.hpp"
#include "engine/graphics/IRenderer.hpp"
#include "engine/network/Client.hpp"
#include "engine/network/Transport.hpp"

using engine::app::ClientApp;
using engine::app::ClientAppOptions;
using engine::app::ServerApp;
using engine::audio::MusicId;
using engine::audio::SoundId;
using engine::core::Vec2;
using engine::core::time::ManualClock;
using engine::graphics::Color;
using engine::graphics::Key;
using engine::graphics::MouseButton;
using engine::graphics::Rect;
using engine::graphics::SpriteDraw;
using engine::graphics::TextureId;
using engine::network::ConnectionState;
using engine::network::LoopbackNetwork;

namespace {

const std::filesystem::path game = std::filesystem::path(RTYPE_FIXTURES_DIR) / "app" / "game";
constexpr int session_frames = 10;

class FakeRenderer final : public fixture::NullRenderer {
public:
    bool open(const engine::graphics::WindowConfig& config) override {
        title = config.title;
        is_open = open_succeeds;
        return open_succeeds;
    }
    void close() override { is_open = false; }
    [[nodiscard]] bool should_close() const override { return frames >= close_after; }
    [[nodiscard]] TextureId load_texture(const std::string& path) override {
        loaded.push_back(path);
        if (on_load) {
            on_load();
        }
        return path.ends_with("missing.png") ? TextureId{} : TextureId{.value = 1};
    }
    [[nodiscard]] Vec2 texture_size(TextureId) const override { return {8, 8}; }
    void begin_frame(Color) override {
        calls.clear();
        rects.clear();
        rects_in_world.clear();
        sprites.clear();
        texts.clear();
        sprites_in_world.clear();
        texts_in_world.clear();
        text_positions.clear();
        views.clear();
    }
    void end_frame() override {
        in_world = false;
        ++frames;
    }
    void begin_world(const engine::core::CameraView& view) override {
        views.push_back(view);
        in_world = true;
    }
    void end_world() override { in_world = false; }
    void draw_sprite(const SpriteDraw& sprite) override {
        calls.emplace_back("sprite");
        sprites.push_back(sprite);
        sprites_in_world.push_back(in_world);
    }
    void draw_rect(const Rect& rect, Color, bool) override {
        calls.emplace_back("rect");
        rects.push_back(rect);
        rects_in_world.push_back(in_world);
    }
    void draw_text(const std::string& text, Vec2 position, float, Color) override {
        calls.push_back("text " + text);
        texts.push_back(text);
        texts_in_world.push_back(in_world);
        text_positions.push_back(position);
    }

    bool open_succeeds{true};
    bool is_open{false};
    int close_after{1'000'000};
    int frames{0};
    std::string title;
    std::function<void()> on_load;
    std::vector<std::string> loaded;
    std::vector<std::string> calls;
    std::vector<Rect> rects;
    std::vector<bool> rects_in_world;
    std::vector<SpriteDraw> sprites;
    std::vector<std::string> texts;
    std::vector<bool> texts_in_world;
    std::vector<Vec2> text_positions;
    std::vector<bool> sprites_in_world;
    std::vector<engine::core::CameraView> views;
    bool in_world{false};
};

class FakeInput final : public engine::graphics::IInput {
public:
    [[nodiscard]] bool is_key_down(Key key) const override { return held.contains(key); }
    [[nodiscard]] bool is_key_pressed(Key) const override { return false; }
    [[nodiscard]] bool is_key_released(Key) const override { return false; }
    [[nodiscard]] Key last_key_pressed() override { return Key::Unknown; }
    [[nodiscard]] Vec2 mouse_position() const override { return {}; }
    [[nodiscard]] bool is_mouse_down(MouseButton) const override { return false; }
    [[nodiscard]] bool is_mouse_pressed(MouseButton) const override { return false; }
    [[nodiscard]] bool is_mouse_released(MouseButton) const override { return false; }

    std::set<Key> held;
};

class FakeAudio final : public engine::audio::IAudio {
public:
    bool open() override { return true; }
    void close() override {}
    [[nodiscard]] bool is_open() const override { return true; }
    void update() override {}
    [[nodiscard]] SoundId load_sound(const std::filesystem::path& path) override {
        loaded.push_back(path.string());
        return {.value = 1};
    }
    void unload_sound(SoundId) override {}
    void play_sound(SoundId, float, float) override {}
    [[nodiscard]] MusicId load_music(const std::filesystem::path& path) override {
        loaded.push_back(path.string());
        return {.value = 1};
    }
    void unload_music(MusicId) override {}
    void play_music(MusicId, bool) override {}
    void stop_music() override {}
    void set_master_volume(float volume) override { master = volume; }
    void set_music_volume(float) override {}
    void set_effects_volume(float) override {}

    float master{-1.F};
    std::vector<std::string> loaded;
};

std::string asset(const std::string& name) { return (game / "assets" / name).string(); }

std::vector<std::string> sorted(std::vector<std::string> names) {
    std::ranges::sort(names);
    return names;
}

class ClientAppTest : public ::testing::Test {
protected:
    ManualClock _clock;
    LoopbackNetwork _loopback;
    FakeRenderer _renderer;
    FakeInput _input;
    FakeAudio _audio;
    ServerApp _server{_clock, _loopback.server(), {.game = game, .max_players = 4}};
    ClientApp _client{_clock,
                      _loopback.add_client(),
                      _renderer,
                      _input,
                      _audio,
                      ClientAppOptions{.scripts = game / "scripts",
                                       .assets = game / "assets",
                                       .window = {.title = "Test game"},
                                       .background = {},
                                       .audio = {}}};

    void SetUp() override {
        std::error_code ec;
        ASSERT_TRUE(_server.start(ec)) << ec.message();
    }

    void start_client() {
        std::string error;
        ASSERT_TRUE(_client.start(error)) << error;
    }

    void play(int frames) {
        for (int frame = 0; frame < frames; ++frame) {
            _clock.advance(std::chrono::milliseconds(17));
            _server.update();
            _client.frame();
        }
    }
};

} // namespace

TEST_F(ClientAppTest, StartOpensTheWindowAndConnects) {
    start_client();

    EXPECT_TRUE(_renderer.is_open);
    EXPECT_EQ(_renderer.title, "Test game");
    play(session_frames);
    EXPECT_EQ(_client.network().state(), ConnectionState::connected);
}

TEST_F(ClientAppTest, DrawsTheEntitiesOfTheServer) {
    start_client();

    play(session_frames);

    ASSERT_EQ(_renderer.sprites.size(), 1U);
    EXPECT_FLOAT_EQ(_renderer.sprites.front().dest.x - _renderer.sprites.front().origin.x, 10.F);
    EXPECT_FLOAT_EQ(_renderer.sprites.front().dest.y - _renderer.sprites.front().origin.y, 20.F);
    EXPECT_EQ(_renderer.sprites.front().tint, (Color{.r = 10, .g = 20, .b = 30, .a = 255}));
}

TEST_F(ClientAppTest, ShowsTheLoadingScreenUntilEveryAssetIsLoaded) {
    start_client();
    std::vector<std::string> progress;
    bool snapshot_hidden = false;

    for (int frame = 0; frame < session_frames && _renderer.sprites.empty(); ++frame) {
        play(1);
        progress.insert(progress.end(), _renderer.texts.begin(), _renderer.texts.end());
        snapshot_hidden |= _renderer.sprites.empty() && !_client.network().entities().empty();
    }

    EXPECT_EQ(progress, (std::vector<std::string>{"0 / 0", "4 / 8"}));
    EXPECT_TRUE(snapshot_hidden);
    EXPECT_EQ(_renderer.sprites.size(), 1U);
    EXPECT_TRUE(_renderer.texts.empty());
}

TEST_F(ClientAppTest, EveryAssetOfTheServerIsLoadedBeforeTheFirstGameFrame) {
    start_client();

    for (int frame = 0; frame < session_frames && _renderer.sprites.empty(); ++frame) {
        play(1);
    }

    ASSERT_EQ(_renderer.sprites.size(), 1U);
    EXPECT_EQ(
        sorted(_renderer.loaded),
        sorted({asset("shapes/square.png"), asset("backgrounds/stars.png"), asset("ui/bar.png"),
                asset("ui/missing.png"), asset("enemies/bug.png"), asset("enemies/boss.png")}));
    EXPECT_EQ(_audio.loaded,
              (std::vector<std::string>{asset("sounds/shot.wav"), asset("music/stage.ogg")}));
}

TEST_F(ClientAppTest, AMissingAssetIsTriedOnceAndTheGameStillStarts) {
    start_client();

    play(session_frames);

    EXPECT_EQ(std::ranges::count(_renderer.loaded, asset("ui/missing.png")), 1);
    EXPECT_EQ(_renderer.sprites.size(), 1U);
}

TEST_F(ClientAppTest, StaysConnectedWhileSlowAssetsLoad) {
    _renderer.on_load = [this] {
        _clock.advance(std::chrono::seconds(1));
        _server.update();
    };
    start_client();

    play(session_frames);

    EXPECT_EQ(_client.network().state(), ConnectionState::connected);
    EXPECT_EQ(_server.network().player_count(), 1U);
    EXPECT_EQ(_renderer.sprites.size(), 1U);
}

TEST_F(ClientAppTest, WithoutCameraTheWorldIsDrawnThroughTheDefaultView) {
    start_client();

    play(session_frames);

    ASSERT_EQ(_renderer.views.size(), 1U);
    EXPECT_EQ(_renderer.views.front(), engine::core::default_view({1920.F, 1080.F}));
    ASSERT_EQ(_renderer.sprites_in_world.size(), _renderer.sprites.size());
    for (const bool in_world : _renderer.sprites_in_world) {
        EXPECT_TRUE(in_world);
    }
    EXPECT_FALSE(_renderer.in_world);
}

TEST_F(ClientAppTest, SceneCameraIsUsedAndNotDrawn) {
    start_client();
    ASSERT_TRUE(_server.script().spawn("view").has_value());

    play(session_frames);

    ASSERT_EQ(_renderer.views.size(), 1U);
    EXPECT_EQ(_renderer.views.front(),
              (engine::core::CameraView{.center = {400.F, 300.F}, .zoom = 2.F}));
    ASSERT_EQ(_renderer.sprites.size(), 1U);
    EXPECT_FLOAT_EQ(_renderer.sprites.front().dest.x - _renderer.sprites.front().origin.x, 10.F);
}

TEST_F(ClientAppTest, ClientCameraOverrideIsLocal) {
    FakeRenderer other_renderer;
    ClientApp other{_clock,
                    _loopback.add_client(),
                    other_renderer,
                    _input,
                    _audio,
                    ClientAppOptions{.scripts = game / "scripts",
                                     .assets = game / "assets",
                                     .window = {},
                                     .background = {},
                                     .audio = {}}};
    std::string error;
    ASSERT_TRUE(other.start(error)) << error;
    start_client();
    ASSERT_TRUE(_server.script().spawn("view").has_value());
    ASSERT_TRUE(_client.script().run_string("camera.set(50, 60, 0.5)", "override"));

    for (int frame = 0; frame < session_frames; ++frame) {
        _clock.advance(std::chrono::milliseconds(17));
        _server.update();
        _client.frame();
        other.frame();
    }

    EXPECT_EQ(_renderer.views.front(),
              (engine::core::CameraView{.center = {50.F, 60.F}, .zoom = 0.5F}));
    EXPECT_EQ(other_renderer.views.front(),
              (engine::core::CameraView{.center = {400.F, 300.F}, .zoom = 2.F}));
    other.stop();
}

TEST_F(ClientAppTest, CameraResetGoesBackToTheSceneCamera) {
    start_client();
    ASSERT_TRUE(_server.script().spawn("view").has_value());
    ASSERT_TRUE(_client.script().run_string("camera.set(50, 60, 0.5)", "override"));
    play(session_frames);

    ASSERT_TRUE(_client.script().run_string("camera.reset()", "reset"));
    play(1);

    EXPECT_EQ(_renderer.views.front(),
              (engine::core::CameraView{.center = {400.F, 300.F}, .zoom = 2.F}));
}

TEST_F(ClientAppTest, ServerEventsReachTheClientScripts) {
    start_client();

    play(session_frames);

    EXPECT_TRUE(_client.script().run_string("assert(pinged == 7)", "check"));
}

TEST_F(ClientAppTest, ClientScriptsReadTheLocalInputAndChangeTheAudio) {
    start_client();
    play(1);
    EXPECT_FLOAT_EQ(_audio.master, 1.F);

    _input.held.insert(Key::M);
    play(3);

    EXPECT_FLOAT_EQ(_audio.master, 0.F);
    EXPECT_TRUE(_client.script_client().audio_settings().muted);
}

TEST_F(ClientAppTest, StartFailsWhenTheWindowCannotOpen) {
    _renderer.open_succeeds = false;
    std::string error;

    EXPECT_FALSE(_client.start(error));

    EXPECT_EQ(error, "cannot open the window");
    EXPECT_EQ(_client.network().state(), ConnectionState::disconnected);
}

TEST_F(ClientAppTest, RunStopsWhenTheWindowClosesAndDisconnects) {
    start_client();
    _renderer.close_after = 3;

    _client.run([] { return false; });

    EXPECT_EQ(_renderer.frames, 3);
    EXPECT_FALSE(_renderer.is_open);
    EXPECT_EQ(_client.network().state(), ConnectionState::disconnected);
}

TEST_F(ClientAppTest, RunStopsWhenAskedTo) {
    start_client();
    int checks = 0;

    _client.run([&checks] { return ++checks > 2; });

    EXPECT_EQ(_renderer.frames, 2);
    EXPECT_FALSE(_renderer.is_open);
}

TEST_F(ClientAppTest, ServerSeesThePlayerLeaveWhenTheClientStops) {
    start_client();
    play(session_frames);
    ASSERT_EQ(_server.network().player_count(), 1U);

    _client.stop();
    _clock.advance(std::chrono::milliseconds(17));
    _server.update();

    EXPECT_EQ(_server.network().player_count(), 0U);
}

TEST_F(ClientAppTest, LocalEntitiesAreDrawnInTheWorldOnlyOnTheirClient) {
    FakeRenderer other_renderer;
    ClientApp other{_clock,
                    _loopback.add_client(),
                    other_renderer,
                    _input,
                    _audio,
                    ClientAppOptions{.scripts = game / "scripts",
                                     .assets = game / "assets",
                                     .window = {},
                                     .background = {},
                                     .audio = {}}};
    std::string error;
    ASSERT_TRUE(other.start(error)) << error;
    start_client();
    ASSERT_TRUE(_client.script().run_string("spawn_local('dust', { x = 100, vx = 0 })", "local"));

    for (int frame = 0; frame < session_frames; ++frame) {
        _clock.advance(std::chrono::milliseconds(17));
        _server.update();
        _client.frame();
        other.frame();
    }

    ASSERT_EQ(_renderer.rects.size(), 1U);
    EXPECT_FLOAT_EQ(_renderer.rects.front().x, 100.F);
    EXPECT_TRUE(_renderer.rects_in_world.front());
    EXPECT_TRUE(other_renderer.rects.empty());
    EXPECT_EQ(other_renderer.sprites.size(), _renderer.sprites.size());
    other.stop();
}

TEST_F(ClientAppTest, LocalEntitiesAreUpdatedByClientScripts) {
    start_client();
    ASSERT_TRUE(_client.script().run_string("spark = spawn_local('spark', { x = 100 })", "local"));

    play(session_frames);

    ASSERT_EQ(_renderer.rects.size(), 1U);
    EXPECT_GT(_renderer.rects.front().x, 100.F);
    EXPECT_TRUE(_client.script().run_string(
        "assert(spark.x == " + std::to_string(static_cast<int>(_renderer.rects.front().x)) + ")",
        "check"));
}

TEST_F(ClientAppTest, LocalEntitiesMoveWithTheirVelocity) {
    start_client();
    ASSERT_TRUE(_client.script().run_string("spawn_local('dust')", "local"));

    play(session_frames);

    ASSERT_EQ(_renderer.rects.size(), 1U);
    EXPECT_GT(_renderer.rects.front().x, 0.F);
}

TEST_F(ClientAppTest, DestroyedLocalEntitiesAreNoLongerDrawn) {
    start_client();
    ASSERT_TRUE(_client.script().run_string("spark = spawn_local('spark')", "local"));
    play(session_frames);
    ASSERT_EQ(_renderer.rects.size(), 1U);

    ASSERT_TRUE(_client.script().run_string("spark:destroy()", "destroy"));
    play(1);

    EXPECT_TRUE(_renderer.rects.empty());
}

TEST_F(ClientAppTest, LocalAndServerEntitiesAreDrawnTogetherByZIndex) {
    start_client();
    ASSERT_TRUE(_client.script().run_string(
        "spawn_local('dust', { vx = 0, z_index = -1 }) spawn_local('spark', { z_index = 1 })",
        "local"));

    play(session_frames);

    EXPECT_EQ(_renderer.calls, (std::vector<std::string>{"rect", "sprite", "rect"}));
}

TEST_F(ClientAppTest, HudIsDrawnAfterTheWorldInScreenSpace) {
    start_client();
    ASSERT_TRUE(
        _client.script().run_string("function hud() draw.text('HP', 10, 20, 16) end", "hud"));

    play(session_frames);

    ASSERT_EQ(_renderer.texts, (std::vector<std::string>{"HP"}));
    EXPECT_FALSE(_renderer.texts_in_world.front());
    EXPECT_EQ(_renderer.calls.back(), "text HP");
}

TEST_F(ClientAppTest, HudStaysFixedWhenTheCameraMoves) {
    start_client();
    ASSERT_TRUE(
        _client.script().run_string("function hud() draw.text('HP', 10, 20, 16) end", "hud"));
    play(session_frames);
    const Vec2 before = _renderer.text_positions.front();

    ASSERT_TRUE(_client.script().run_string("camera.set(900, 700, 3)", "move"));
    play(1);

    EXPECT_EQ(_renderer.views.front(),
              (engine::core::CameraView{.center = {900.F, 700.F}, .zoom = 3.F}));
    EXPECT_EQ(_renderer.text_positions.front(), before);
}

TEST_F(ClientAppTest, AnErrorInTheHudIsLoggedAndTheFrameGoesOn) {
    start_client();
    ASSERT_TRUE(_client.script().run_string(
        "function hud() draw.text('HP', 10, 20, 16) error('broken hud') end", "hud"));

    play(session_frames);

    EXPECT_EQ(_renderer.texts, (std::vector<std::string>{"HP"}));
    EXPECT_EQ(_renderer.sprites.size(), 1U);
    EXPECT_EQ(_renderer.frames, session_frames);
}

TEST_F(ClientAppTest, NoHudIsDrawnWhileLoading) {
    ASSERT_TRUE(
        _client.script().run_string("function hud() draw.text('HP', 10, 20, 16) end", "hud"));
    start_client();

    _client.frame();

    EXPECT_TRUE(std::ranges::none_of(_renderer.texts,
                                     [](const std::string& text) { return text == "HP"; }));
}

TEST_F(ClientAppTest, MetricsRecordTheFrameTimeAndFps) {
    start_client();

    play(session_frames);

    const auto frame = _client.metrics().summary("frame");
    ASSERT_TRUE(frame.has_value());
    EXPECT_DOUBLE_EQ(frame->last, 17.0);
    EXPECT_EQ(frame->samples, static_cast<std::size_t>(session_frames - 1));
    EXPECT_NEAR(_client.metrics().summary("fps")->last, 1000.0 / 17.0, 1e-9);
}

TEST_F(ClientAppTest, MetricsDescribeTheGameAndTheNetwork) {
    constexpr int frames_over_two_seconds = 120;
    start_client();

    play(frames_over_two_seconds);

    const auto& metrics = _client.metrics();
    const auto names = metrics.names();
    const std::set<std::string> recorded(names.begin(), names.end());
    for (const char* name :
         {"frame", "fps", "network.update", "lua.update", "draw", "entities", "network.rtt",
          "network.packets_lost", "network.packets_in", "network.packets_out", "network.bytes_in",
          "network.bytes_out", "network.snapshots"}) {
        EXPECT_TRUE(recorded.contains(name)) << name;
    }
    EXPECT_DOUBLE_EQ(metrics.summary("entities")->last, 1.0);
    EXPECT_GT(metrics.summary("network.bytes_in")->last, 0.0);
    EXPECT_GT(metrics.summary("network.packets_out")->last, 0.0);
    EXPECT_GT(metrics.summary("network.snapshots")->max, 0.0);
    EXPECT_DOUBLE_EQ(_server.metrics().summary("players")->last, 1.0);
    EXPECT_GT(_server.metrics().summary("snapshot.bytes")->last, 0.0);
}

TEST_F(ClientAppTest, DisabledMetricsRecordNothing) {
    _client.metrics().set_enabled(false);
    start_client();

    play(session_frames);

    EXPECT_TRUE(_client.metrics().names().empty());
}
