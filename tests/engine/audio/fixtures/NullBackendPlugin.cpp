#include <cstdint>

#include "engine/audio/PluginApi.hpp"

ENGINE_AUDIO_PLUGIN_EXPORT std::uint32_t engine_audio_api_version() {
    return engine::audio::plugin_api_version;
}

ENGINE_AUDIO_PLUGIN_EXPORT engine::audio::IAudio* engine_audio_create() { return nullptr; }

ENGINE_AUDIO_PLUGIN_EXPORT void engine_audio_destroy(engine::audio::IAudio* backend) {
    delete backend;
}
