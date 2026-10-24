#pragma once

#include <cstdint>
#include <string_view>

#include "engine/audio/IAudio.hpp"

namespace engine::audio {

/**
 * @brief Version of the audio plugin interface.
 *
 * Incremented on every change to IAudio or to the entry points. The plugin loader rejects any
 * plugin whose `engine_audio_api_version()` returns a different value.
 */
inline constexpr std::uint32_t plugin_api_version = 1;

/** @brief `extern "C" std::uint32_t engine_audio_api_version()`: returns plugin_api_version. */
using ApiVersionFunction = std::uint32_t (*)();

/**
 * @brief `extern "C" IAudio* engine_audio_create()`: allocates the backend.
 *
 * @note It must not open the audio device: the caller does it with IAudio::open().
 */
using CreateFunction = IAudio* (*)();

/** @brief `extern "C" void engine_audio_destroy(IAudio*)`: frees the backend. */
using DestroyFunction = void (*)(IAudio*);

/** @brief Exported symbol names an audio plugin must provide. */
inline constexpr std::string_view api_version_symbol = "engine_audio_api_version";
inline constexpr std::string_view create_symbol = "engine_audio_create";
inline constexpr std::string_view destroy_symbol = "engine_audio_destroy";

} // namespace engine::audio

/**
 * @brief Marks an audio plugin entry point: C linkage, exported from the plugin library.
 *
 * Usage: `ENGINE_AUDIO_PLUGIN_EXPORT std::uint32_t engine_audio_api_version() { ... }`.
 */
#if defined(_WIN32)
#define ENGINE_AUDIO_PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
#define ENGINE_AUDIO_PLUGIN_EXPORT extern "C" __attribute__((visibility("default")))
#endif
