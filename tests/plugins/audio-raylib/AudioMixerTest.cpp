#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include "AudioMixer.hpp"

TEST(AudioMixer, VolumesStartAtFull) {
    const AudioMixer mixer;

    EXPECT_FLOAT_EQ(mixer.effect_volume(1.F), 1.F);
    EXPECT_FLOAT_EQ(mixer.music_volume(), 1.F);
}

TEST(AudioMixer, EffectVolumeMultipliesRequestEffectsAndMaster) {
    AudioMixer mixer;
    mixer.set_effects_volume(0.5F);
    mixer.set_master_volume(0.5F);

    EXPECT_FLOAT_EQ(mixer.effect_volume(0.8F), 0.2F);
}

TEST(AudioMixer, MusicVolumeIgnoresEffectsVolume) {
    AudioMixer mixer;
    mixer.set_effects_volume(0.F);
    mixer.set_music_volume(0.5F);
    mixer.set_master_volume(0.5F);

    EXPECT_FLOAT_EQ(mixer.music_volume(), 0.25F);
    EXPECT_FLOAT_EQ(mixer.effect_volume(1.F), 0.F);
}

TEST(AudioMixer, ClampsEveryVolume) {
    AudioMixer mixer;
    mixer.set_master_volume(3.F);
    mixer.set_music_volume(-1.F);
    mixer.set_effects_volume(2.F);

    EXPECT_FLOAT_EQ(mixer.effect_volume(5.F), 1.F);
    EXPECT_FLOAT_EQ(mixer.effect_volume(-5.F), 0.F);
    EXPECT_FLOAT_EQ(mixer.music_volume(), 0.F);
}

TEST(AudioMixer, NotANumberVolumeIsSilent) {
    AudioMixer mixer;
    mixer.set_master_volume(std::numeric_limits<float>::quiet_NaN());

    EXPECT_FLOAT_EQ(mixer.music_volume(), 0.F);
    EXPECT_FLOAT_EQ(clamp_volume(std::numeric_limits<float>::quiet_NaN()), 0.F);
}

TEST(AudioMixer, ClampsPitchToAPlayableRange) {
    EXPECT_FLOAT_EQ(clamp_pitch(1.F), 1.F);
    EXPECT_FLOAT_EQ(clamp_pitch(0.F), 0.25F);
    EXPECT_FLOAT_EQ(clamp_pitch(-2.F), 0.25F);
    EXPECT_FLOAT_EQ(clamp_pitch(10.F), 4.F);
    EXPECT_FLOAT_EQ(clamp_pitch(std::numeric_limits<float>::quiet_NaN()), 1.F);
}

TEST(VoiceCycle, GoesThroughEveryVoiceInTurn) {
    VoiceCycle cycle(3);

    EXPECT_EQ(cycle.next(), 0U);
    EXPECT_EQ(cycle.next(), 1U);
    EXPECT_EQ(cycle.next(), 2U);
    EXPECT_EQ(cycle.next(), 0U);
}

TEST(VoiceCycle, WithoutVoicesAlwaysGivesTheFirst) {
    VoiceCycle cycle(0);

    EXPECT_EQ(cycle.next(), 0U);
    EXPECT_EQ(cycle.next(), 0U);
}
