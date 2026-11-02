#include "engine/app/Launch.hpp"

#include <exception>
#include <stop_token>
#include <string_view>
#include <system_error>
#include <thread>

#include "engine/app/ClientApp.hpp"
#include "engine/app/ServerApp.hpp"
#include "engine/audio/NullAudio.hpp"
#include "engine/audio/PluginLoader.hpp"
#include "engine/core/log/Log.hpp"
#include "engine/core/time/SteadyClock.hpp"
#include "engine/graphics/PluginLoader.hpp"
#include "engine/network/Transport.hpp"

namespace engine::app {

namespace {

namespace log = core::log;

constexpr std::string_view local_address = "127.0.0.1";

ServerAppOptions server_options(const LaunchOptions& options) {
    return {.scripts = options.game / "scripts", .max_players = options.max_players};
}

ClientAppOptions client_options(const LaunchOptions& options) {
    return {.scripts = options.game / "scripts",
            .assets = options.game / "assets",
            .window = options.window,
            .background = {.r = 0, .g = 0, .b = 0, .a = 255},
            .audio = options.audio};
}

bool start_server(ServerApp& server, const network::UdpServerTransport& transport) {
    std::error_code ec;
    if (!server.start(ec)) {
        log::error("server: cannot listen on UDP port {}: {}", transport.port(), ec.message());
        return false;
    }
    log::info("server: listening on UDP port {}", transport.port());
    return true;
}

bool run_dedicated(const LaunchOptions& options, const std::function<bool()>& should_stop) {
    const core::time::SteadyClock clock;
    network::UdpServerTransport transport(options.port);
    ServerApp server(clock, transport, server_options(options));
    if (!start_server(server, transport)) {
        return false;
    }
    server.run(should_stop);
    return true;
}

class Backends {
public:
    explicit Backends(const LaunchOptions& options)
        : _graphics(graphics::load_graphics_plugin(options.graphics_plugin)),
          _sound(audio::load_audio_plugin(options.audio_plugin)) {
        if (_graphics.plugin) {
            _renderer = &_graphics.plugin->backend().renderer();
            _input = &_graphics.plugin->backend().input();
        } else {
            log::error("client: {}", _graphics.error);
        }
        if (_sound.plugin) {
            _audio = &_sound.plugin->backend();
        } else {
            log::warn("client: {}: sound is disabled", _sound.error);
        }
    }

    [[nodiscard]] bool loaded() const { return _renderer != nullptr; }
    [[nodiscard]] graphics::IRenderer& renderer() const { return *_renderer; }
    [[nodiscard]] const graphics::IInput& input() const { return *_input; }
    [[nodiscard]] audio::IAudio& audio() const { return *_audio; }

private:
    graphics::PluginLoadResult _graphics;
    audio::PluginLoadResult _sound;
    audio::NullAudio _silence;
    graphics::IRenderer* _renderer{nullptr};
    const graphics::IInput* _input{nullptr};
    audio::IAudio* _audio{&_silence};
};

bool start_client(ClientApp& client) {
    std::string error;
    if (!client.start(error)) {
        log::error("client: {}", error);
        return false;
    }
    return true;
}

bool play(ClientApp& client, const std::function<bool()>& should_stop) {
    if (!start_client(client)) {
        return false;
    }
    client.run(should_stop);
    return true;
}

bool run_client(const LaunchOptions& options, const std::function<bool()>& should_stop) {
    Backends backends(options);
    if (!backends.loaded()) {
        return false;
    }
    const core::time::SteadyClock clock;
    network::UdpClientTransport transport(options.server_ip, options.port);
    ClientApp client(clock, transport, backends.renderer(), backends.input(), backends.audio(),
                     client_options(options));
    return play(client, should_stop);
}

bool run_host(const LaunchOptions& options, const std::function<bool()>& should_stop) {
    Backends backends(options);
    if (!backends.loaded()) {
        return false;
    }
    const core::time::SteadyClock clock;
    network::UdpServerTransport server_transport(options.port);
    ServerApp server(clock, server_transport, server_options(options));
    if (!start_server(server, server_transport)) {
        return false;
    }
    network::UdpClientTransport client_transport(std::string(local_address),
                                                 server_transport.port());
    ClientApp client(clock, client_transport, backends.renderer(), backends.input(),
                     backends.audio(), client_options(options));
    std::jthread server_thread([&server](const std::stop_token& token) {
        server.run([&token] { return token.stop_requested(); });
    });
    return play(client, should_stop);
}

bool run_solo(const LaunchOptions& options, const std::function<bool()>& should_stop) {
    Backends backends(options);
    if (!backends.loaded()) {
        return false;
    }
    const core::time::SteadyClock clock;
    network::LoopbackNetwork loopback;
    ServerApp server(clock, loopback.server(), server_options(options));
    ClientApp client(clock, loopback.add_client(), backends.renderer(), backends.input(),
                     backends.audio(), client_options(options));
    std::error_code ec;
    if (!server.start(ec)) {
        log::error("server: cannot start: {}", ec.message());
        return false;
    }
    if (!start_client(client)) {
        return false;
    }
    while (!client.should_close() && !should_stop()) {
        server.update();
        client.frame();
    }
    client.stop();
    server.stop();
    return true;
}

} // namespace

bool launch(const LaunchOptions& options, const std::function<bool()>& should_stop) noexcept {
    try {
        switch (options.mode) {
        case Mode::dedicated:
            return run_dedicated(options, should_stop);
        case Mode::client:
            return run_client(options, should_stop);
        case Mode::host:
            return run_host(options, should_stop);
        case Mode::solo:
            return run_solo(options, should_stop);
        }
    } catch (const std::exception& exception) {
        log::error("cannot run the game: {}", exception.what());
    }
    return false;
}

} // namespace engine::app
