#pragma once

#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <system_error>

#include "engine/app/Export.hpp"
#include "engine/core/EntityManager.hpp"
#include "engine/core/SystemScheduler.hpp"
#include "engine/core/event/EventBus.hpp"
#include "engine/core/time/IClock.hpp"
#include "engine/network/Server.hpp"
#include "engine/network/StringTable.hpp"
#include "engine/network/Transport.hpp"
#include "engine/script/ScriptRuntime.hpp"
#include "engine/script/ServerApi.hpp"

namespace engine::app {

/** @brief Settings of a ServerApp. */
struct ServerAppOptions {
    /** @brief Script folder of the game, holding its `main.lua`. Empty runs no script. */
    std::filesystem::path scripts;
    /** @brief Maximum number of players connected at once. */
    std::size_t max_players{4};
};

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

/**
 * @brief Runs the authoritative simulation of any game folder and serves it to clients.
 *
 * Loads the scripts with the server API set, builds the string table from their declarations,
 * then on every tick: handles the received packets, hands each player's input to the scripts,
 * updates the scripts, runs physics and reports collisions to the scripts, removes destroyed
 * entities, runs the systems, then sends the queued audio commands, events and a snapshot of
 * every entity spawned by the scripts.
 *
 * Works the same over any transport: UDP for a dedicated or self-hosted server, a
 * LoopbackNetwork for a solo game or a test.
 *
 * @note Not thread safe: every call must come from the same thread. Script errors are logged
 * and never stop the server.
 */
class ENGINE_APP_EXPORT ServerApp {
public:
    /**
     * @param clock Time source of the tick loop and of the network timeouts; must outlive this
     * app.
     * @param transport Carries the packets; must outlive this app.
     * @param options Game folder and player limit.
     */
    ServerApp(const core::time::IClock& clock, network::ServerTransport& transport,
              const ServerAppOptions& options);
    ~ServerApp();

    ServerApp(const ServerApp&) = delete;
    ServerApp& operator=(const ServerApp&) = delete;
    ServerApp(ServerApp&&) = delete;
    ServerApp& operator=(ServerApp&&) = delete;

    /**
     * @brief Opens the transport.
     *
     * @param ec Set to the reason on failure, such as a port already in use.
     * @return true if clients can now connect.
     */
    bool start(std::error_code& ec);

    /** @brief Runs one simulation tick of @p step. */
    void tick(core::time::Duration step);

    /**
     * @brief Runs as many fixed ticks as the clock advanced since the previous call.
     *
     * Ticks are 1/60 s; a long pause is caught up by at most a few ticks.
     */
    void update();

    /**
     * @brief Calls update() until @p should_stop returns true, then stop().
     *
     * Sleeps about one millisecond between two calls so an idle server does not spin.
     */
    void run(const std::function<bool()>& should_stop);

    /** @brief Disconnects every player and closes the transport. */
    void stop();

    /** @return The entities of the simulation. */
    [[nodiscard]] core::EntityManager& world() noexcept;
    /** @return The systems run once per tick, after the scripts and physics. */
    [[nodiscard]] core::SystemScheduler& systems() noexcept;
    /** @return The bus carrying network and physics events. */
    [[nodiscard]] core::event::EventBus& events() noexcept;
    /** @return The network server. */
    [[nodiscard]] network::NetworkServer& network() noexcept;
    /** @return The script runtime. */
    [[nodiscard]] script::ScriptRuntime& script() noexcept;
    /** @return The server API set of the script runtime. */
    [[nodiscard]] script::ServerApi& script_server() noexcept;
    /** @return The names sent to clients when they connect. */
    [[nodiscard]] const network::StringTable& strings() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

} // namespace engine::app
