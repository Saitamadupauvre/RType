#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <string>

#include "engine/core/InputState.hpp"
#include "engine/core/Keys.hpp"
#include "engine/core/Vec2.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"
#include "engine/script/ClientApi.hpp"
#include "engine/script/EventCommand.hpp"
#include "engine/script/ScriptRuntime.hpp"

namespace logger = engine::core::log;
using engine::core::InputState;
using engine::core::Key;
using engine::core::MouseButton;
using engine::core::Vec2;
using engine::script::AudioSettings;
using engine::script::ClientApi;
using engine::script::EventCommand;
using engine::script::ScriptRuntime;

namespace {

std::filesystem::path client_fixture() {
    return std::filesystem::path(RTYPE_FIXTURES_DIR) / "scripts" / "api_sets" / "client";
}

EventCommand hit(float x, float y, int damage) {
    return EventCommand{
        .name = "hit",
        .arguments = {{.key = "at", .value = Vec2{x, y}}, {.key = "damage", .value = damage}}};
}

InputState holding(Key key) {
    return InputState{.keys = {key}, .mouse_buttons = {}, .pointer = {}};
}

class ClientApiTest : public ::testing::Test {
protected:
    std::shared_ptr<logger::MemorySink> _sink = std::make_shared<logger::MemorySink>();
    logger::ScopedSink _scope{_sink, logger::Level::Trace};
    ScriptRuntime _runtime = ScriptRuntime::builder().with<ClientApi>().build();
    ClientApi& _client = _runtime.api<ClientApi>();

    void SetUp() override { ASSERT_TRUE(_runtime.load_scripts(client_fixture())); }

    [[nodiscard]] bool logged_error(const std::string& part) const {
        for (const auto& record : _sink->records()) {
            if (record.level == logger::Level::Error &&
                record.message.find(part) != std::string::npos) {
                return true;
            }
        }
        return false;
    }
};

} // namespace

TEST_F(ClientApiTest, DispatchCallsHandlerWithArguments) {
    ASSERT_TRUE(_runtime.run_string("on('hit', function(event) last = event end)", "client"));

    EXPECT_TRUE(_client.dispatch(hit(1.0F, 2.0F, 3)));

    EXPECT_TRUE(_runtime.run_string(
        "assert(last.damage == 3 and last.at.x == 1 and last.at.y == 2)", "check"));
}

TEST_F(ClientApiTest, DispatchRunsHandlersInRegistrationOrder) {
    ASSERT_TRUE(_runtime.run_string(R"(
        order = ""
        on("hit", function() order = order .. "a" end)
        on("hit", function() order = order .. "b" end)
    )",
                                    "client"));

    EXPECT_TRUE(_client.dispatch(hit(0.0F, 0.0F, 1)));

    EXPECT_TRUE(_runtime.run_string("assert(order == 'ab')", "check"));
}

TEST_F(ClientApiTest, DispatchWithoutHandlerSucceeds) {
    EXPECT_TRUE(_client.dispatch(EventCommand{.name = "quiet", .arguments = {}}));
}

TEST_F(ClientApiTest, DispatchGivesEachHandlerItsOwnTable) {
    ASSERT_TRUE(_runtime.run_string(R"(
        on("hit", function(event) event.damage = 99 end)
        on("hit", function(event) seen = event.damage end)
    )",
                                    "client"));

    EXPECT_TRUE(_client.dispatch(hit(0.0F, 0.0F, 4)));

    EXPECT_TRUE(_runtime.run_string("assert(seen == 4)", "check"));
}

TEST_F(ClientApiTest, FailingHandlerIsLoggedAndOthersStillRun) {
    ASSERT_TRUE(_runtime.run_string(R"(
        on("hit", function() error("boom") end)
        on("hit", function() reached = true end)
    )",
                                    "handlers.lua"));

    EXPECT_FALSE(_client.dispatch(hit(0.0F, 0.0F, 1)));

    EXPECT_TRUE(logged_error("handlers.lua:2: boom"));
    EXPECT_TRUE(_runtime.run_string("assert(reached == true)", "check"));
}

TEST_F(ClientApiTest, OnRejectsUndeclaredEvent) {
    EXPECT_FALSE(_runtime.run_string("on('nope', function() end)", "client.lua"));

    EXPECT_TRUE(logged_error("client.lua:1: on: unknown event 'nope', declare it with event"));
}

TEST_F(ClientApiTest, OnRejectsHandlerThatIsNotAFunction) {
    EXPECT_FALSE(_runtime.run_string("on('hit', 3)", "client.lua"));

    EXPECT_TRUE(logged_error("client.lua:1: on: handler of event 'hit' must be a function"));
}

TEST_F(ClientApiTest, OnRejectsNameThatIsNotAString) {
    EXPECT_FALSE(_runtime.run_string("on(4, function() end)", "client.lua"));

    EXPECT_TRUE(logged_error("client.lua:1: on: event name must be a string"));
}

TEST_F(ClientApiTest, InputReadsTheLastSetInput) {
    _client.set_input(InputState{
        .keys = {Key::Space}, .mouse_buttons = {MouseButton::Left}, .pointer = Vec2{3.F, 4.F}});

    EXPECT_TRUE(_runtime.run_string(R"(
        assert(input:key("Space") and not input:key("A"))
        assert(input:mouse("Left") and not input:mouse("Right"))
        local x, y = input:pointer()
        assert(x == 3 and y == 4)
    )",
                                    "check"));
}

TEST_F(ClientApiTest, InputStartsNeutral) {
    EXPECT_TRUE(_runtime.run_string(
        "assert(not input:key('Space') and not input:pressed('Space') and input:pointer() == nil)",
        "check"));
}

TEST_F(ClientApiTest, PressedIsTrueOnlyWhenTheKeyGoesDown) {
    _client.set_input(holding(Key::M));
    EXPECT_TRUE(_runtime.run_string("assert(input:pressed('M'))", "first"));

    _client.set_input(holding(Key::M));
    EXPECT_TRUE(_runtime.run_string("assert(input:key('M') and not input:pressed('M'))", "held"));

    _client.set_input(InputState{});
    _client.set_input(holding(Key::M));
    EXPECT_TRUE(_runtime.run_string("assert(input:pressed('M'))", "again"));
}

TEST_F(ClientApiTest, InputRejectsUnknownNames) {
    EXPECT_FALSE(_runtime.run_string("input:pressed('Nope')", "client"));
    EXPECT_TRUE(logged_error("input:pressed: unknown key 'Nope'"));
    EXPECT_FALSE(_runtime.run_string("input:key(3)", "client"));
    EXPECT_TRUE(logged_error("input:key: unknown key of type number"));
    EXPECT_FALSE(_runtime.run_string("input:mouse('Middle-ish')", "client"));
    EXPECT_TRUE(logged_error("input:mouse: unknown mouse button 'Middle-ish'"));
}

TEST_F(ClientApiTest, AudioReadsTheSettings) {
    _client.set_audio_settings(
        AudioSettings{.music_volume = 0.25F, .effects_volume = 0.5F, .muted = true});

    EXPECT_TRUE(_runtime.run_string(R"(
        assert(audio.muted())
        assert(audio.volume("music") == 0.25 and audio.volume("effects") == 0.5)
    )",
                                    "check"));
}

TEST_F(ClientApiTest, AudioChangesTheSettings) {
    ASSERT_TRUE(_runtime.run_string(R"(
        audio.set_muted(true)
        audio.set_volume("music", 0.5)
        audio.set_volume("effects", 0)
    )",
                                    "client"));

    EXPECT_EQ(_client.audio_settings(),
              (AudioSettings{.music_volume = 0.5F, .effects_volume = 0.F, .muted = true}));
}

TEST_F(ClientApiTest, AudioRejectsInvalidArgumentsAndKeepsTheSettings) {
    EXPECT_FALSE(_runtime.run_string("audio.set_volume('music', 1.5)", "client"));
    EXPECT_TRUE(logged_error("audio.set_volume: volume must be a number from 0 to 1"));
    EXPECT_FALSE(_runtime.run_string("audio.set_volume('music', 0/0)", "client"));
    EXPECT_FALSE(_runtime.run_string("audio.set_volume('voices', 0.5)", "client"));
    EXPECT_TRUE(logged_error("audio.set_volume: kind must be \"music\" or \"effects\""));
    EXPECT_FALSE(_runtime.run_string("audio.volume(nil)", "client"));
    EXPECT_TRUE(logged_error("audio.volume: kind must be"));
    EXPECT_FALSE(_runtime.run_string("audio.set_muted('yes')", "client"));
    EXPECT_TRUE(logged_error("audio.set_muted: argument must be a boolean"));

    EXPECT_EQ(_client.audio_settings(), AudioSettings{});
}

TEST_F(ClientApiTest, SetAudioSettingsClampsVolumes) {
    _client.set_audio_settings(AudioSettings{.music_volume = 2.F, .effects_volume = -1.F});

    EXPECT_EQ(_client.audio_settings(),
              (AudioSettings{.music_volume = 1.F, .effects_volume = 0.F, .muted = false}));
}
