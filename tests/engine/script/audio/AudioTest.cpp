#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "engine/core/EntityManager.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/core/log/MemorySink.hpp"
#include "engine/script/AudioCommand.hpp"
#include "engine/script/ScriptRuntime.hpp"
#include "engine/script/ServerApi.hpp"

namespace logger = engine::core::log;
using engine::core::EntityManager;
using engine::script::AudioCommand;
using engine::script::AudioCommandKind;
using engine::script::ScriptRuntime;
using engine::script::ServerApi;

namespace {

std::filesystem::path audio_fixture(const std::string& name) {
    return std::filesystem::path(RTYPE_FIXTURES_DIR) / "scripts" / "audio" / name;
}

AudioCommand sound(const std::string& name) {
    return {.kind = AudioCommandKind::Sound, .name = name, .source = std::nullopt};
}

AudioCommand music(const std::string& name) {
    return {.kind = AudioCommandKind::Music, .name = name, .source = std::nullopt};
}

AudioCommand stop_music() {
    return {.kind = AudioCommandKind::StopMusic, .name = "", .source = std::nullopt};
}

class AudioTest : public ::testing::Test {
protected:
    std::shared_ptr<logger::MemorySink> _sink = std::make_shared<logger::MemorySink>();
    logger::ScopedSink _scope{_sink, logger::Level::Trace};
    EntityManager _world;
    ScriptRuntime _runtime = ScriptRuntime::builder().with<ServerApi>().build();
    ServerApi& _server = _runtime.api<ServerApi>();

    void SetUp() override { _runtime.attach_world(_world); }

    [[nodiscard]] std::size_t count_logged(logger::Level level, const std::string& part) const {
        return static_cast<std::size_t>(
            std::ranges::count_if(_sink->records(), [&](const logger::MemoryRecord& record) {
                return record.level == level && record.message.find(part) != std::string::npos;
            }));
    }
};

} // namespace

TEST_F(AudioTest, ListsDeclaredNamesInAscendingOrder) {
    ASSERT_TRUE(_runtime.load_scripts(audio_fixture("declared")));

    EXPECT_EQ(_runtime.sound_names(), (std::vector<std::string>{"explosion.wav", "shot.wav"}));
    EXPECT_EQ(_runtime.music_names(), (std::vector<std::string>{"boss.mp3", "stage1.ogg"}));
}

TEST_F(AudioTest, NothingIsDeclaredWithoutScripts) {
    EXPECT_TRUE(_runtime.sound_names().empty());
    EXPECT_TRUE(_runtime.music_names().empty());
    EXPECT_TRUE(_server.take_audio_commands().empty());
}

TEST_F(AudioTest, QueuesCommandsInCallOrder) {
    ASSERT_TRUE(_runtime.load_scripts(audio_fixture("declared")));

    ASSERT_TRUE(_runtime.run_string(
        R"(play("shot.wav") music("boss.mp3") play("explosion.wav") music(nil) music())", "audio"));

    EXPECT_EQ(_server.take_audio_commands(),
              (std::vector<AudioCommand>{sound("shot.wav"), music("boss.mp3"),
                                         sound("explosion.wav"), stop_music(), stop_music()}));
}

TEST_F(AudioTest, TakingCommandsEmptiesTheQueue) {
    ASSERT_TRUE(_runtime.load_scripts(audio_fixture("declared")));
    ASSERT_TRUE(_runtime.run_string(R"(play("shot.wav"))", "audio"));

    EXPECT_EQ(_server.take_audio_commands().size(), 1U);
    EXPECT_TRUE(_server.take_audio_commands().empty());
}

TEST_F(AudioTest, PlayRecordsTheSourceEntity) {
    ASSERT_TRUE(_runtime.load_scripts(audio_fixture("declared")));
    const auto ship = _runtime.spawn("ship");
    ASSERT_TRUE(ship.has_value());

    ASSERT_TRUE(_runtime.call(*ship, "fire"));

    EXPECT_EQ(_server.take_audio_commands(),
              (std::vector<AudioCommand>{
                  {.kind = AudioCommandKind::Sound, .name = "shot.wav", .source = ship}}));
}

TEST_F(AudioTest, MusicStartedByOnStartIsQueued) {
    ASSERT_TRUE(_runtime.load_scripts(audio_fixture("on_start")));

    EXPECT_EQ(_server.take_audio_commands(), (std::vector<AudioCommand>{music("stage1.ogg")}));
}

TEST_F(AudioTest, UnknownNamesRaiseAnErrorAndQueueNothing) {
    ASSERT_TRUE(_runtime.load_scripts(audio_fixture("declared")));

    EXPECT_FALSE(_runtime.run_string(R"(play("missing.wav"))", "audio"));
    EXPECT_FALSE(_runtime.run_string(R"(music("shot.wav"))", "audio"));

    EXPECT_TRUE(_server.take_audio_commands().empty());
    EXPECT_EQ(count_logged(logger::Level::Error, "unknown sound 'missing.wav'"), 1U);
    EXPECT_EQ(count_logged(logger::Level::Error, "unknown music 'shot.wav'"), 1U);
}

TEST_F(AudioTest, WrongArgumentTypesRaiseAnError) {
    ASSERT_TRUE(_runtime.load_scripts(audio_fixture("declared")));

    EXPECT_FALSE(_runtime.run_string("play(42)", "audio"));
    EXPECT_FALSE(_runtime.run_string("play()", "audio"));
    EXPECT_FALSE(_runtime.run_string(R"(play("shot.wav", 3))", "audio"));
    EXPECT_FALSE(_runtime.run_string("music(true)", "audio"));

    EXPECT_TRUE(_server.take_audio_commands().empty());
    EXPECT_EQ(count_logged(logger::Level::Error, "name must be a string"), 3U);
    EXPECT_EQ(count_logged(logger::Level::Error, "source must be an entity or nil"), 1U);
}

TEST_F(AudioTest, DeclaringAfterLoadingFails) {
    ASSERT_TRUE(_runtime.load_scripts(audio_fixture("declared")));

    EXPECT_FALSE(_runtime.run_string(R"(sounds { "late.wav" })", "audio"));

    EXPECT_EQ(_runtime.sound_names(), (std::vector<std::string>{"explosion.wav", "shot.wav"}));
    EXPECT_EQ(count_logged(logger::Level::Error, "can only be called while the scripts load"), 1U);
}

TEST_F(AudioTest, DeclaringOutsideLoadScriptsFails) {
    EXPECT_FALSE(_runtime.run_string(R"(musics { "stage1.ogg" })", "audio"));

    EXPECT_TRUE(_runtime.music_names().empty());
}

TEST_F(AudioTest, RejectsNamesThatAreNotPlainFileNames) {
    EXPECT_FALSE(_runtime.load_scripts(audio_fixture("invalid_name")));

    EXPECT_TRUE(_runtime.sound_names().empty());
    EXPECT_EQ(count_logged(logger::Level::Error, "'../secret.wav' is not a valid file name"), 1U);
}

TEST_F(AudioTest, RejectsADuplicateAndKeepsNoNameOfThatCall) {
    EXPECT_FALSE(_runtime.load_scripts(audio_fixture("duplicate")));

    EXPECT_EQ(_runtime.sound_names(), (std::vector<std::string>{"shot.wav"}));
    EXPECT_EQ(count_logged(logger::Level::Error, "'shot.wav' is already declared"), 1U);
}

TEST_F(AudioTest, RejectsADeclarationThatIsNotAList) {
    EXPECT_FALSE(_runtime.load_scripts(audio_fixture("not_a_list")));

    EXPECT_TRUE(_runtime.music_names().empty());
    EXPECT_EQ(count_logged(logger::Level::Error, "musics expects a list of file names"), 1U);
}

TEST_F(AudioTest, DropsCommandsBeyondTheLimitAndWarnsOnce) {
    ASSERT_TRUE(_runtime.load_scripts(audio_fixture("declared")));

    ASSERT_TRUE(_runtime.run_string(R"(for _ = 1, 100 do play("shot.wav") end)", "audio"));

    EXPECT_EQ(_server.take_audio_commands().size(), 64U);
    EXPECT_EQ(count_logged(logger::Level::Warn, "dropping the rest"), 1U);
}

TEST_F(AudioTest, WarnsAgainAfterTheQueueWasTaken) {
    ASSERT_TRUE(_runtime.load_scripts(audio_fixture("declared")));
    const std::string flood = R"(for _ = 1, 65 do play("shot.wav") end)";

    ASSERT_TRUE(_runtime.run_string(flood, "audio"));
    (void)_server.take_audio_commands();
    ASSERT_TRUE(_runtime.run_string(flood, "audio"));

    EXPECT_EQ(count_logged(logger::Level::Warn, "dropping the rest"), 2U);
}
