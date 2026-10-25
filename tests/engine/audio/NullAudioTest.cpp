#include <gtest/gtest.h>

#include <memory>
#include <type_traits>

#include "engine/audio/IAudio.hpp"
#include "engine/audio/NullAudio.hpp"
#include "engine/audio/PluginApi.hpp"
#include "engine/audio/Types.hpp"

using namespace engine::audio;

TEST(AudioTypes, DefaultIdsReferToNothing) {
    EXPECT_EQ(SoundId{}.value, 0U);
    EXPECT_EQ(MusicId{}.value, 0U);
    EXPECT_NE(SoundId{}, SoundId{.value = 1});
    EXPECT_NE(MusicId{}, MusicId{.value = 1});
}

TEST(AudioInterface, IsNotCopyableOrMovable) {
    static_assert(!std::is_copy_constructible_v<IAudio>);
    static_assert(!std::is_move_constructible_v<IAudio>);
    SUCCEED();
}

TEST(AudioPluginApi, SymbolNamesAreStable) {
    EXPECT_EQ(api_version_symbol, "engine_audio_api_version");
    EXPECT_EQ(create_symbol, "engine_audio_create");
    EXPECT_EQ(destroy_symbol, "engine_audio_destroy");
}

TEST(NullAudio, IsClosedUntilOpened) {
    NullAudio audio;

    EXPECT_FALSE(audio.is_open());
    EXPECT_EQ(audio.load_sound("missing.wav"), SoundId{});
    EXPECT_EQ(audio.load_music("missing.ogg"), MusicId{});
}

TEST(NullAudio, HandsOutDistinctIdsOnceOpen) {
    NullAudio audio;
    ASSERT_TRUE(audio.open());

    const SoundId first = audio.load_sound("missing.wav");
    const SoundId second = audio.load_sound("missing.wav");
    const MusicId music = audio.load_music("missing.ogg");

    EXPECT_NE(first, SoundId{});
    EXPECT_NE(second, SoundId{});
    EXPECT_NE(first, second);
    EXPECT_NE(music, MusicId{});
}

TEST(NullAudio, AcceptsEveryCallThroughTheInterface) {
    std::unique_ptr<IAudio> audio = std::make_unique<NullAudio>();
    ASSERT_TRUE(audio->open());
    const SoundId sound = audio->load_sound("shot.wav");
    const MusicId music = audio->load_music("stage.ogg");

    audio->set_master_volume(2.F);
    audio->set_music_volume(-1.F);
    audio->set_effects_volume(0.5F);
    audio->play_sound(sound, 1.F, 1.F);
    audio->play_sound(SoundId{.value = 999}, 1.F, 1.F);
    audio->play_music(music, true);
    audio->update();
    audio->stop_music();
    audio->unload_sound(sound);
    audio->unload_music(music);
    audio->close();

    EXPECT_FALSE(audio->is_open());
}
