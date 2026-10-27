#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

#include "AudioPlayer.hpp"
#include "engine/audio/IAudio.hpp"
#include "engine/network/Audio.hpp"
#include "engine/network/StringTable.hpp"

using engine::audio::MusicId;
using engine::audio::SoundId;
using engine::network::StringCategory;
using engine::network::StringTable;
using rtype::client::AudioPlayer;
using rtype::client::AudioSettings;
using rtype::client::is_plain_file_name;

namespace {

class RecordingAudio final : public engine::audio::IAudio {
public:
    bool open() override { return open_succeeds; }
    void close() override {}
    [[nodiscard]] bool is_open() const override { return open_succeeds; }
    void update() override {}
    [[nodiscard]] SoundId load_sound(const std::filesystem::path& path) override {
        loaded.push_back(path.generic_string());
        return fail_loads ? SoundId{} : SoundId{static_cast<std::uint32_t>(loaded.size())};
    }
    void unload_sound(SoundId) override {}
    void play_sound(SoundId sound, float, float) override { played.push_back(sound.value); }
    [[nodiscard]] MusicId load_music(const std::filesystem::path& path) override {
        loaded.push_back(path.generic_string());
        return fail_loads ? MusicId{} : MusicId{static_cast<std::uint32_t>(loaded.size())};
    }
    void unload_music(MusicId) override {}
    void play_music(MusicId music, bool loop) override {
        music_played.push_back(music.value);
        looped = loop;
    }
    void stop_music() override { ++stops; }
    void set_master_volume(float volume) override { master = volume; }
    void set_music_volume(float volume) override { music_volume = volume; }
    void set_effects_volume(float volume) override { effects_volume = volume; }

    bool open_succeeds{true};
    bool fail_loads{false};
    std::vector<std::string> loaded;
    std::vector<std::uint32_t> played;
    std::vector<std::uint32_t> music_played;
    bool looped{false};
    int stops{0};
    float master{-1.F};
    float music_volume{-1.F};
    float effects_volume{-1.F};
};

StringTable strings_of(StringCategory category, const std::vector<std::string>& names) {
    StringTable strings;
    for (const std::string& name : names) {
        strings.add(category, name);
    }
    return strings;
}

const StringTable sounds = strings_of(StringCategory::sound, {"shot.wav", "boom.wav"});
const StringTable musics = strings_of(StringCategory::music, {"stage.ogg"});

} // namespace

TEST(AudioPlayer, AppliesTheSettingsVolumes) {
    RecordingAudio audio;

    const AudioPlayer player(audio, "assets",
                             AudioSettings{.music_volume = 0.3F, .effects_volume = 0.6F});

    EXPECT_FLOAT_EQ(audio.music_volume, 0.3F);
    EXPECT_FLOAT_EQ(audio.effects_volume, 0.6F);
    EXPECT_FLOAT_EQ(audio.master, 1.F);
    EXPECT_FALSE(player.muted());
}

TEST(AudioPlayer, StartsMutedWhenAsked) {
    RecordingAudio audio;

    const AudioPlayer player(audio, "assets", AudioSettings{.muted = true});

    EXPECT_FLOAT_EQ(audio.master, 0.F);
    EXPECT_TRUE(player.muted());
}

TEST(AudioPlayer, ToggleMuteSwitchesTheMasterVolume) {
    RecordingAudio audio;
    AudioPlayer player(audio, "assets", AudioSettings{});

    player.toggle_mute();
    EXPECT_TRUE(player.muted());
    EXPECT_FLOAT_EQ(audio.master, 0.F);
    player.toggle_mute();
    EXPECT_FALSE(player.muted());
    EXPECT_FLOAT_EQ(audio.master, 1.F);
}

TEST(AudioPlayer, LoadsASoundFromTheSoundFolderOnceAndPlaysIt) {
    RecordingAudio audio;
    AudioPlayer player(audio, "assets", AudioSettings{});

    player.play_sound(sounds, 2);
    player.play_sound(sounds, 2);

    EXPECT_EQ(audio.loaded, (std::vector<std::string>{"assets/sounds/boom.wav"}));
    EXPECT_EQ(audio.played, (std::vector<std::uint32_t>{1, 1}));
}

TEST(AudioPlayer, PlaysTheMusicInALoopFromTheMusicFolder) {
    RecordingAudio audio;
    AudioPlayer player(audio, "assets", AudioSettings{});

    player.change_music(musics, 1);

    EXPECT_EQ(audio.loaded, (std::vector<std::string>{"assets/music/stage.ogg"}));
    EXPECT_EQ(audio.music_played, (std::vector<std::uint32_t>{1}));
    EXPECT_TRUE(audio.looped);
}

TEST(AudioPlayer, NoMusicStopsTheMusic) {
    RecordingAudio audio;
    AudioPlayer player(audio, "assets", AudioSettings{});

    player.change_music(musics, engine::network::no_music);

    EXPECT_EQ(audio.stops, 1);
    EXPECT_TRUE(audio.loaded.empty());
}

TEST(AudioPlayer, IgnoresIdsMissingFromTheTable) {
    RecordingAudio audio;
    AudioPlayer player(audio, "assets", AudioSettings{});

    player.play_sound(sounds, 9);
    player.change_music(musics, 9);

    EXPECT_TRUE(audio.loaded.empty());
    EXPECT_TRUE(audio.played.empty());
    EXPECT_TRUE(audio.music_played.empty());
}

TEST(AudioPlayer, NeverLoadsANameThatLeavesTheAssetFolder) {
    RecordingAudio audio;
    AudioPlayer player(audio, "assets", AudioSettings{});
    const StringTable hostile =
        strings_of(StringCategory::sound, {"../../secret.wav", "sub/shot.wav", ".hidden"});

    player.play_sound(hostile, 1);
    player.play_sound(hostile, 2);
    player.play_sound(hostile, 3);

    EXPECT_TRUE(audio.loaded.empty());
}

TEST(AudioPlayer, DoesNotRetryAFileThatFailedToLoad) {
    RecordingAudio audio;
    audio.fail_loads = true;
    AudioPlayer player(audio, "assets", AudioSettings{});

    player.play_sound(sounds, 1);
    player.play_sound(sounds, 1);

    EXPECT_EQ(audio.loaded.size(), 1U);
    EXPECT_TRUE(audio.played.empty());
}

TEST(AudioPlayer, LoadsNothingWhileTheDeviceIsClosed) {
    RecordingAudio audio;
    audio.open_succeeds = false;
    AudioPlayer player(audio, "assets", AudioSettings{});

    player.play_sound(sounds, 1);
    player.change_music(musics, 1);

    EXPECT_TRUE(audio.loaded.empty());
}

TEST(AudioPlayer, PlainFileNamesAreLettersDigitsAndSeparators) {
    EXPECT_TRUE(is_plain_file_name("laser_shoot.wav"));
    EXPECT_TRUE(is_plain_file_name("bit-space.mp3"));
    EXPECT_FALSE(is_plain_file_name(""));
    EXPECT_FALSE(is_plain_file_name(".."));
    EXPECT_FALSE(is_plain_file_name("a/b.wav"));
    EXPECT_FALSE(is_plain_file_name("a\\b.wav"));
    EXPECT_FALSE(is_plain_file_name("a b.wav"));
}
