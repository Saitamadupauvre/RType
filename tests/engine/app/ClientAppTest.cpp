#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <set>
#include <string>
#include <system_error>
#include <vector>

#include "engine/app/ClientApp.hpp"
#include "engine/app/ServerApp.hpp"
#include "engine/audio/IAudio.hpp"
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

class FakeRenderer final : public engine::graphics::IRenderer {
public:
    bool open(const engine::graphics::WindowConfig& config) override {
        title = config.title;
        is_open = open_succeeds;
        return open_succeeds;
    }
    void close() override { is_open = false; }
    [[nodiscard]] bool should_close() const override { return frames >= close_after; }
    void set_fullscreen(bool) override {}
    [[nodiscard]] Vec2 virtual_size() const override { return {1920, 1080}; }
    [[nodiscard]] TextureId load_texture(const std::string&) override { return {.value = 1}; }
    void unload_texture(TextureId) override {}
    [[nodiscard]] Vec2 texture_size(TextureId) const override { return {8, 8}; }
    void begin_frame(Color) override { sprites.clear(); }
    void end_frame() override { ++frames; }
    void draw_sprite(const SpriteDraw& sprite) override { sprites.push_back(sprite); }
    void draw_rect(const Rect&, Color, bool) override {}
    void draw_circle(Vec2, float, Color) override {}
    void draw_line(Vec2, Vec2, float, Color) override {}
    void draw_text(const std::string&, Vec2, float, Color) override {}
    [[nodiscard]] Vec2 measure_text(const std::string&, float) const override { return {}; }

    bool open_succeeds{true};
    bool is_open{false};
    int close_after{1'000'000};
    int frames{0};
    std::string title;
    std::vector<SpriteDraw> sprites;
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
    [[nodiscard]] SoundId load_sound(const std::filesystem::path&) override { return {}; }
    void unload_sound(SoundId) override {}
    void play_sound(SoundId, float, float) override {}
    [[nodiscard]] MusicId load_music(const std::filesystem::path&) override { return {}; }
    void unload_music(MusicId) override {}
    void play_music(MusicId, bool) override {}
    void stop_music() override {}
    void set_master_volume(float volume) override { master = volume; }
    void set_music_volume(float) override {}
    void set_effects_volume(float) override {}

    float master{-1.F};
};

class ClientAppTest : public ::testing::Test {
protected:
    ManualClock _clock;
    LoopbackNetwork _loopback;
    FakeRenderer _renderer;
    FakeInput _input;
    FakeAudio _audio;
    ServerApp _server{_clock, _loopback.server(), {.scripts = game / "scripts", .max_players = 4}};
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
    EXPECT_FLOAT_EQ(_renderer.sprites.front().dest.x, 10.F);
    EXPECT_FLOAT_EQ(_renderer.sprites.front().dest.y, 20.F);
    EXPECT_EQ(_renderer.sprites.front().tint, (Color{.r = 10, .g = 20, .b = 30, .a = 255}));
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
