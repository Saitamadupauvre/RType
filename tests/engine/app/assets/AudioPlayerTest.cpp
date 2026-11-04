#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

#include "assets/AudioPlayer.hpp"
#include "engine/audio/IAudio.hpp"
#include "engine/network/Audio.hpp"
#include "engine/network/StringTable.hpp"
#include "engine/script/ClientApi.hpp"

using engine::app::AudioPlayer;
using engine::audio::MusicId;
using engine::audio::SoundId;
using engine::network::StringCategory;
using engine::network::StringTable;
using engine::script::AudioSettings;

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
    void unload_sound(SoundId) override { ++unloaded_sounds; }
    void play_sound(SoundId sound, float, float) override { played.push_back(sound.value); }
    [[nodiscard]] MusicId load_music(const std::filesystem::path& path) override {
        loaded.push_back(path.generic_string());
        return fail_loads ? MusicId{} : MusicId{static_cast<std::uint32_t>(loaded.size())};
    }
    void unload_music(MusicId) override { ++unloaded_musics; }
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
    int unloaded_sounds{0};
    int unloaded_musics{0};
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
    AudioPlayer player(audio, "assets");

    player.apply(AudioSettings{.music_volume = 0.3F, .effects_volume = 0.6F, .muted = false});

    EXPECT_FLOAT_EQ(audio.music_volume, 0.3F);
    EXPECT_FLOAT_EQ(audio.effects_volume, 0.6F);
    EXPECT_FLOAT_EQ(audio.master, 1.F);
}

TEST(AudioPlayer, MutingSilencesTheMasterVolume) {
    RecordingAudio audio;
    AudioPlayer player(audio, "assets");

    player.apply(AudioSettings{.music_volume = 1.F, .effects_volume = 1.F, .muted = true});
    EXPECT_FLOAT_EQ(audio.master, 0.F);
    player.apply(AudioSettings{.music_volume = 1.F, .effects_volume = 1.F, .muted = false});
    EXPECT_FLOAT_EQ(audio.master, 1.F);
}

TEST(AudioPlayer, LeavesTheDeviceAloneWhenTheSettingsDidNotChange) {
    RecordingAudio audio;
    AudioPlayer player(audio, "assets");
    player.apply(AudioSettings{});
    audio.master = -1.F;

    player.apply(AudioSettings{});

    EXPECT_FLOAT_EQ(audio.master, -1.F);
}

TEST(AudioPlayer, LoadsASoundFromTheSoundFolderOnceAndPlaysIt) {
    RecordingAudio audio;
    AudioPlayer player(audio, "assets");

    player.play_sound(sounds, 2);
    player.play_sound(sounds, 2);

    EXPECT_EQ(audio.loaded, (std::vector<std::string>{"assets/sounds/boom.wav"}));
    EXPECT_EQ(audio.played, (std::vector<std::uint32_t>{1, 1}));
}

TEST(AudioPlayer, PlaysTheMusicInALoopFromTheMusicFolder) {
    RecordingAudio audio;
    AudioPlayer player(audio, "assets");

    player.change_music(musics, 1);

    EXPECT_EQ(audio.loaded, (std::vector<std::string>{"assets/music/stage.ogg"}));
    EXPECT_EQ(audio.music_played, (std::vector<std::uint32_t>{1}));
    EXPECT_TRUE(audio.looped);
}

TEST(AudioPlayer, NoMusicStopsTheMusic) {
    RecordingAudio audio;
    AudioPlayer player(audio, "assets");

    player.change_music(musics, engine::network::no_music);

    EXPECT_EQ(audio.stops, 1);
    EXPECT_TRUE(audio.loaded.empty());
}

TEST(AudioPlayer, IgnoresIdsMissingFromTheTable) {
    RecordingAudio audio;
    AudioPlayer player(audio, "assets");

    player.play_sound(sounds, 9);
    player.change_music(musics, 9);

    EXPECT_TRUE(audio.loaded.empty());
    EXPECT_TRUE(audio.played.empty());
    EXPECT_TRUE(audio.music_played.empty());
}

TEST(AudioPlayer, NeverLoadsANameThatLeavesTheAssetFolder) {
    RecordingAudio audio;
    AudioPlayer player(audio, "assets");
    const StringTable hostile =
        strings_of(StringCategory::sound, {"../../secret.wav", "/etc/secret.wav", ".hidden"});

    player.play_sound(hostile, 1);
    player.play_sound(hostile, 2);
    player.play_sound(hostile, 3);

    EXPECT_TRUE(audio.loaded.empty());
}

TEST(AudioPlayer, LoadsASoundFromASubfolderOfTheSoundFolder) {
    RecordingAudio audio;
    AudioPlayer player(audio, "assets");

    player.play_sound(strings_of(StringCategory::sound, {"enemies/boom.wav"}), 1);

    EXPECT_EQ(audio.loaded, (std::vector<std::string>{"assets/sounds/enemies/boom.wav"}));
}

TEST(AudioPlayer, UnloadsEveryLoadedSoundAndMusicWhenDestroyed) {
    RecordingAudio audio;
    {
        AudioPlayer player(audio, "assets");
        player.play_sound(sounds, 1);
        player.change_music(musics, 1);
    }

    EXPECT_EQ(audio.unloaded_sounds, 1);
    EXPECT_EQ(audio.unloaded_musics, 1);
}

TEST(AudioPlayer, DoesNotRetryAFileThatFailedToLoad) {
    RecordingAudio audio;
    audio.fail_loads = true;
    AudioPlayer player(audio, "assets");

    player.play_sound(sounds, 1);
    player.play_sound(sounds, 1);

    EXPECT_EQ(audio.loaded.size(), 1U);
    EXPECT_TRUE(audio.played.empty());
}

TEST(AudioPlayer, LoadsNothingWhileTheDeviceIsClosed) {
    RecordingAudio audio;
    audio.open_succeeds = false;
    AudioPlayer player(audio, "assets");

    player.play_sound(sounds, 1);
    player.change_music(musics, 1);

    EXPECT_TRUE(audio.loaded.empty());
}

TEST(AudioPlayer, PreloadedSoundAndMusicAreNotLoadedAgainWhenPlayed) {
    RecordingAudio audio;
    AudioPlayer player(audio, "assets");

    EXPECT_TRUE(player.preload_sound("shot.wav"));
    EXPECT_TRUE(player.preload_music("stage.ogg"));
    player.play_sound(sounds, 1);
    player.change_music(musics, 1);

    EXPECT_EQ(audio.loaded,
              (std::vector<std::string>{"assets/sounds/shot.wav", "assets/music/stage.ogg"}));
    EXPECT_EQ(audio.played, (std::vector<std::uint32_t>{1}));
    EXPECT_EQ(audio.music_played, (std::vector<std::uint32_t>{2}));
}

TEST(AudioPlayer, PreloadReportsAMissingSoundOrMusic) {
    RecordingAudio audio;
    audio.fail_loads = true;
    AudioPlayer player(audio, "assets");

    EXPECT_FALSE(player.preload_sound("shot.wav"));
    EXPECT_FALSE(player.preload_music("stage.ogg"));
    EXPECT_FALSE(player.preload_sound("../secret.wav"));
}

TEST(AudioPlayer, PreloadSkipsAudioWhileTheDeviceIsClosed) {
    RecordingAudio audio;
    audio.open_succeeds = false;
    AudioPlayer player(audio, "assets");

    EXPECT_TRUE(player.preload_sound("shot.wav"));
    EXPECT_TRUE(player.preload_music("stage.ogg"));

    EXPECT_TRUE(audio.loaded.empty());
}
