#include "engine/network/Server.hpp"

#include <utility>

#include "engine/core/log/Log.hpp"
#include "engine/network/Events.hpp"

#include "event/GameEvents.hpp"
#include "protocol/Validation.hpp"
#include "server/ServerNetwork.hpp"
#include "socket/UdpSocket.hpp"
#include "thread/NetworkIo.hpp"

namespace engine::network {

namespace {

ServerSessionsConfig sessions_config(const ServerOptions& options) {
    return ServerSessionsConfig{.max_players = options.max_players, .strings = options.strings};
}

} // namespace

struct NetworkServer::ImplNet {
    ImplNet(const core::time::IClock& clock, core::event::EventBus& game_events,
            ServerOptions server_options)
        : options(std::move(server_options)), events(game_events),
          network(clock, game_events, sessions_config(options)) {
        forward<EntitySpawned>();
        forward<EntityDestroyed>();
        forward<ShotFired>();
        forward<SoundPlayed>();
        forward<MusicChanged>();
        subscriptions.push_back(events.get().subscribe<MusicChanged>(
            [this](const MusicChanged& changed) { network.set_current_music(changed.music_id); }));
    }

    ~ImplNet() {
        for (const auto subscription : subscriptions) {
            events.get().unsubscribe(subscription);
        }
    }

    ImplNet(const ImplNet&) = delete;
    ImplNet& operator=(const ImplNet&) = delete;
    ImplNet(ImplNet&&) = delete;
    ImplNet& operator=(ImplNet&&) = delete;

    template <typename Event> void forward() {
        subscriptions.push_back(events.get().subscribe<Event>(
            [this](const Event& event) { network.queue_event(to_payload(event)); }));
    }

    void send_all(std::vector<Outbound> outbound) {
        for (auto& message : outbound) {
            io.send(std::move(message));
        }
    }

    ServerOptions options;
    std::reference_wrapper<core::event::EventBus> events;
    std::vector<core::event::Subscription> subscriptions;
    ServerNetwork network;
    NetworkIo io;
};

NetworkServer::NetworkServer(const core::time::IClock& clock, core::event::EventBus& events,
                             ServerOptions options)
    : impl_(std::make_unique<ImplNet>(clock, events, std::move(options))) {}

NetworkServer::~NetworkServer() = default;

bool NetworkServer::start(std::error_code& ec) {
    ec.clear();
    const auto config = sessions_config(impl_->options);
    if (!is_valid(ConnectAcceptPayload{.strings = config.strings})) {
        core::log::error("network: the string table does not fit in one packet");
        ec = std::make_error_code(std::errc::invalid_argument);
        return false;
    }
    const auto any_address = Endpoint::from_string("0.0.0.0", impl_->options.port);
    impl_->io.start(any_address.value_or(Endpoint{}), ec);
    return !ec;
}

void NetworkServer::stop() {
    impl_->send_all(impl_->network.shutdown());
    impl_->io.stop();
}

void NetworkServer::receive() { impl_->send_all(impl_->network.receive(impl_->io.poll())); }

std::vector<PlayerInput> NetworkServer::inputs() const {
    std::vector<PlayerInput> inputs;
    for (const auto& session : impl_->network.sessions()) {
        inputs.push_back(PlayerInput{.player_id = session.player_id,
                                     .action_flags = session.input.action_flags,
                                     .move_x = session.input.move_x,
                                     .move_y = session.input.move_y});
    }
    return inputs;
}

void NetworkServer::send_snapshot(core::EntityManager& registry) {
    impl_->send_all(impl_->network.snapshot(registry));
}

std::uint16_t NetworkServer::port() const {
    std::error_code ec;
    const auto local = impl_->io.local_endpoint(ec);
    return ec ? impl_->options.port : local.port();
}

std::size_t NetworkServer::player_count() const { return impl_->network.sessions().size(); }

std::vector<PlayerStats> NetworkServer::stats() const { return impl_->network.stats(); }

} // namespace engine::network
