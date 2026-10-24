#pragma once

#include <filesystem>

#include "engine/audio/IAudio.hpp"

namespace fixture {

class Audio final : public engine::audio::IAudio {
public:
    bool open() override { return true; }
    void close() override {}
    [[nodiscard]] bool is_open() const override { return false; }
    void update() override {}
    [[nodiscard]] engine::audio::SoundId load_sound(const std::filesystem::path&) override {
        return {};
    }
    void unload_sound(engine::audio::SoundId) override {}
    void play_sound(engine::audio::SoundId, float, float) override {}
    [[nodiscard]] engine::audio::MusicId load_music(const std::filesystem::path&) override {
        return {};
    }
    void unload_music(engine::audio::MusicId) override {}
    void play_music(engine::audio::MusicId, bool) override {}
    void stop_music() override {}
    void set_master_volume(float) override {}
    void set_music_volume(float) override {}
    void set_effects_volume(float) override {}
};

} // namespace fixture
