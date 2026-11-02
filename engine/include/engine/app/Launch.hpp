#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

#include "engine/app/Export.hpp"
#include "engine/graphics/Types.hpp"
#include "engine/script/ClientApi.hpp"

namespace engine::app {

/** @brief How a game process runs. */
enum class Mode : std::uint8_t {
    /** @brief Server only, without window, listening on UDP. */
    dedicated,
    /** @brief Client of a server reached over UDP. */
    client,
    /** @brief Server on a background thread listening on UDP, and a client playing on it. */
    host,
    /** @brief Server and client in the same thread over a LoopbackNetwork: no socket. */
    solo,
};

/** @brief Everything launch() needs to run a game folder in one mode. */
struct LaunchOptions {
    Mode mode{Mode::solo};
    /** @brief Game folder, holding `scripts/` and `assets/`. */
    std::filesystem::path game;
    /** @brief Graphics plugin file; unused by Mode::dedicated. */
    std::filesystem::path graphics_plugin;
    /** @brief Audio plugin file; when it cannot be loaded the game runs silent. */
    std::filesystem::path audio_plugin;
    /** @brief Numeric address of the server, for Mode::client. */
    std::string server_ip;
    /** @brief UDP port to connect to (Mode::client) or listen on (dedicated, host). */
    std::uint16_t port{0};
    /** @brief Maximum number of players connected at once, for every mode but client. */
    std::size_t max_players{4};
    /** @brief Window opened by the client. */
    graphics::WindowConfig window;
    /** @brief Audio levels the client scripts start with. */
    script::AudioSettings audio;
};

/**
 * @brief Runs a game folder in the chosen mode until it ends.
 *
 * A client ends when its window is closed, a dedicated server when @p should_stop returns true;
 * both also end when @p should_stop returns true. On start, a server logs the port it listens on.
 *
 * @param options Mode, game folder and settings.
 * @param should_stop Polled about once per frame or tick; typically set by a signal handler.
 * @return false if the game could not start: missing graphics plugin, port in use, server
 * unreachable, window not created. The reason is logged.
 * @note Never throws.
 */
[[nodiscard]] ENGINE_APP_EXPORT bool launch(const LaunchOptions& options,
                                            const std::function<bool()>& should_stop) noexcept;

} // namespace engine::app
