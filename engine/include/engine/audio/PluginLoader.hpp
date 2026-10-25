#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "engine/audio/Export.hpp"
#include "engine/audio/IAudio.hpp"
#include "engine/audio/PluginApi.hpp"
#include "engine/core/DynamicLibrary.hpp"

namespace engine::audio {

struct PluginLoadResult;

/**
 * @brief A loaded audio plugin and the backend it created.
 *
 * The backend is destroyed by the plugin's own engine_audio_destroy(), before the library is
 * closed.
 *
 * @note Move-constructible only: move assignment is deleted because it would close the old library
 * before destroying the backend it created. Main thread only.
 */
class AudioPlugin {
public:
    AudioPlugin(const AudioPlugin&) = delete;
    AudioPlugin& operator=(const AudioPlugin&) = delete;
    AudioPlugin(AudioPlugin&&) noexcept = default;
    AudioPlugin& operator=(AudioPlugin&&) = delete;
    ~AudioPlugin() = default;

    /** @return The backend created by the plugin. */
    [[nodiscard]] IAudio& backend() noexcept { return *_backend; }

private:
    using Backend = std::unique_ptr<IAudio, DestroyFunction>;

    AudioPlugin(core::DynamicLibrary library, Backend backend)
        : _library(std::move(library)), _backend(std::move(backend)) {}

    friend ENGINE_AUDIO_EXPORT PluginLoadResult load_audio_plugin(const std::filesystem::path&);

    core::DynamicLibrary _library;
    Backend _backend;
};

/** @brief Outcome of load_audio_plugin(): a plugin, or the reason it was rejected. */
struct PluginLoadResult {
    std::optional<AudioPlugin> plugin;
    std::string error;
};

/**
 * @brief Opens an audio plugin, checks its API version and creates its backend.
 *
 * The audio device is not opened: call IAudio::open() on the backend.
 *
 * @param path Plugin file, used as is; resolving it relative to the executable is the caller's job.
 * @return The plugin, or an empty optional and an error naming the path when the file cannot be
 * opened, an entry point is missing, the API version differs from plugin_api_version, or
 * engine_audio_create() returns null.
 * @note Never throws on a bad plugin.
 */
[[nodiscard]] ENGINE_AUDIO_EXPORT PluginLoadResult
load_audio_plugin(const std::filesystem::path& path);

} // namespace engine::audio
