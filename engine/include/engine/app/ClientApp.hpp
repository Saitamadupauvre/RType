#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <string>

#include "engine/app/Export.hpp"
#include "engine/audio/IAudio.hpp"
#include "engine/core/time/IClock.hpp"
#include "engine/graphics/IInput.hpp"
#include "engine/graphics/IRenderer.hpp"
#include "engine/graphics/Types.hpp"
#include "engine/network/Client.hpp"
#include "engine/network/Transport.hpp"
#include "engine/script/ClientApi.hpp"
#include "engine/script/ScriptRuntime.hpp"

namespace engine::app {

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
 * Loads the scripts with the client API set. Every frame it reads the network, then on each
 * fixed tick of 1/60 s gives the local input to the scripts (`input`), runs their `update(dt)`
 * and sends that input to the server. Events received from the server reach the handlers
 * registered with `on(name, handler)`; audio levels changed with `audio.*` apply on the same
 * frame. Every entity of the latest snapshot is drawn with its sprite
 * `sprites/<type>.png`, from the asset folder.
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
              graphics::IRenderer& renderer, const graphics::IInput& input, audio::IAudio& audio,
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

    /** @brief Runs one frame: network, fixed ticks, audio and drawing. */
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

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

} // namespace engine::app
