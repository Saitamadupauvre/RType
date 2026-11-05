#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <string>

#include "engine/app/Export.hpp"
#include "engine/audio/IAudio.hpp"
#include "engine/core/console/CVars.hpp"
#include "engine/core/console/Console.hpp"
#include "engine/core/metrics/Metrics.hpp"
#include "engine/core/time/IClock.hpp"
#include "engine/graphics/IInput.hpp"
#include "engine/graphics/IRenderer.hpp"
#include "engine/graphics/Types.hpp"
#include "engine/network/Client.hpp"
#include "engine/network/Transport.hpp"
#include "engine/script/ClientApi.hpp"
#include "engine/script/ScriptRuntime.hpp"

namespace engine::app {

class ServerApp;

/** @brief Settings of a ClientApp. */
struct ClientAppOptions {
    /** @brief Script folder of the game, holding its `main.lua`. Empty runs no script. */
    std::filesystem::path scripts;
    /** @brief Asset folder of the game: sprites, `sounds/` and `music/`. */
    std::filesystem::path assets;
    /** @brief Window opened by start(). */
    graphics::WindowConfig window;
    /** @brief Color the window is cleared with every frame. */
    graphics::Color background{.r = 0, .g = 0, .b = 0, .a = 255};
    /** @brief Audio levels scripts start with. */
    script::AudioSettings audio;
};

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

/**
 * @brief Shows any game folder served by a ServerApp: draws the replicated entities, plays the
 * sounds and music the server asks for and sends the local input.
 *
 * Once the server accepts the connection, every texture, sound and music of its string table is
 * preloaded, a few per frame, while a loading screen shows the progress; until then the network
 * keeps running but no tick runs and no snapshot is drawn. A missing asset is logged and drawn as
 * the placeholder.
 *
 * Loads the scripts with the client API set. Every frame it reads the network, then on each
 * fixed tick of 1/60 s gives the local input to the scripts (`input`), runs their `update(dt)`
 * and sends that input to the server. Events received from the server reach the handlers
 * registered with `on(name, handler)`; audio levels changed with `audio.*` apply on the same
 * frame. Every entity of the latest snapshot is drawn with the sprite the server replicates,
 * loaded once from the asset folder; an entity without one is drawn as a coloured square.
 *
 * Entities spawned by client scripts with `spawn_local` live in a world of their own, attached to
 * the scripts before they load: they are never sent to the server, run their `client` section and
 * move with their velocity on every tick, and are removed after the tick that destroys them. They
 * are drawn with the replicated entities, every entity in ascending `z_index`, then spawn order.
 * Then the HUD queued by the `draw_hud()` callback of the scripts is drawn in screen space, in
 * front of the world and unaffected by the camera; an error in it is logged and the frame goes on.
 *
 * @note Not thread safe: every call must come from the thread owning the renderer.
 */
class ENGINE_APP_EXPORT ClientApp {
public:
    /**
     * @param clock Time source of the tick loop and of the network timeouts.
     * @param transport Carries the packets to the server.
     * @param renderer Window and drawing backend.
     * @param input Keyboard and mouse backend.
     * @param audio Audio backend; engine::audio::NullAudio plays nothing.
     * @param options Game folder, window and initial audio levels.
     * @note Every reference must outlive this app.
     */
    ClientApp(const core::time::IClock& clock, network::ClientTransport& transport,
              graphics::IRenderer& renderer, graphics::IInput& input, audio::IAudio& audio,
              ClientAppOptions options);
    ~ClientApp();

    ClientApp(const ClientApp&) = delete;
    ClientApp& operator=(const ClientApp&) = delete;
    ClientApp(ClientApp&&) = delete;
    ClientApp& operator=(ClientApp&&) = delete;

    /**
     * @brief Opens the audio device, starts connecting to the server and opens the window.
     *
     * A missing audio device is logged and the game runs silent.
     *
     * @param error Set to a readable reason on failure.
     * @return false if the transport cannot be opened or the window cannot be created.
     */
    bool start(std::string& error);

    /**
     * @brief Runs one frame: network, then either preloading and the loading screen, or the
     * fixed ticks, audio and drawing of the game.
     */
    void frame();

    /**
     * @brief Calls frame() until the window is closed or @p should_stop returns true, then stop().
     */
    void run(const std::function<bool()>& should_stop);

    /** @brief Disconnects from the server and closes the window. */
    void stop();

    /** @return true once the window was asked to close. */
    [[nodiscard]] bool should_close() const;

    /** @return The network client. */
    [[nodiscard]] network::NetworkClient& network() noexcept;
    /** @return The script runtime. */
    [[nodiscard]] script::ScriptRuntime& script() noexcept;
    /** @return The client API set of the script runtime. */
    [[nodiscard]] script::ClientApi& script_client() noexcept;
    /**
     * @return The timings of every frame: one frame per call to frame().
     *
     * Timers (ms): `frame` (time since the previous frame), `network.update`, `lua.update` and
     * `physics` (local entities), both summed over every tick of the frame, `draw`. Counter:
     * `network.snapshots` (1 on a frame showing a newer snapshot). Gauges: `fps`, `entities` (of
     * the latest snapshot), `network.rtt` (ms), `network.packets_lost` (since connecting),
     * `network.packets_in`, `network.packets_out`, `network.bytes_in`, `network.bytes_out` (per
     * second), `process.cpu` (% of one core) and `process.memory` (MiB).
     * @note Unlike the rest of this class, the returned object may be read from any thread.
     */
    [[nodiscard]] core::metrics::Metrics& metrics() noexcept;

    /**
     * @brief Shows or hides the metrics overlay. F3 also toggles it during frame().
     *
     * The overlay is drawn over everything, in screen space, with the renderer's primitives
     * only: FPS, frame time and its graph, Lua, physics and draw times, entity count, CPU and
     * memory, then the lagometer (ping, lost packets, snapshots per second, bytes in and out).
     * A hidden overlay draws nothing. Hidden by default.
     *
     * @note Same as setting the console variable `debug.metrics`.
     */
    void show_metrics_overlay(bool shown);

    /** @return true while the metrics overlay is drawn. */
    [[nodiscard]] bool metrics_overlay_shown() const noexcept;

    /**
     * @brief Adds the tick, Lua and physics times and the entity count of a server running in
     * the same process to the overlay.
     *
     * @param server The metrics of that server, read from this thread while it may write them
     * from another; must outlive this app or be reset with nullptr. nullptr removes them.
     */
    void show_server_metrics(const core::metrics::Metrics* server) noexcept;

    /**
     * @brief Connects the overlay and the console to a server running in the same process.
     *
     * Shows its metrics like show_server_metrics(), and sends the console lines starting with
     * `/server` to ServerApp::submit_lua(); their results are printed by the next frames.
     *
     * @param server Must outlive this app. It may run on another thread.
     */
    void attach_local_server(ServerApp& server);

    /**
     * @return The developer console, toggled with ` (grave accent) or F1 during frame().
     *
     * While open, it takes the keyboard: the game receives no key, typed characters edit the
     * input line, Enter runs it, Up and Down walk the history, Page Up and Page Down scroll and
     * Escape closes it. Plain lines run as Lua in the client runtime, and every log message is
     * copied to its scrollback.
     */
    [[nodiscard]] core::console::Console& console() noexcept;

    /**
     * @return The console variables of the client, also reachable from its scripts as `cvar`.
     *
     * The engine registers `debug.metrics` and `debug.hitboxes` (bool), `audio.music_volume` and
     * `audio.effects_volume` (number, 0 to 1) and `audio.muted` (bool). The audio variables
     * follow script::ClientApi::audio_settings() both ways.
     */
    [[nodiscard]] core::console::CVars& cvars() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

} // namespace engine::app
